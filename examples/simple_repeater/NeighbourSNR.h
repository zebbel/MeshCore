#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <limits.h>

// No allocation or per-observation persistence. Records own their identity via
// their enclosing slot; a slot is reclaimed only after all references expire.
class NeighbourSNR {
public:
  static constexpr uint8_t REQUEST = 0x70, VERSION = 1;
  static constexpr unsigned ACTIVE = 50, IDENTITIES = 100, HISTORY = 4;
  static constexpr uint32_t PERIOD = 1800, RETENTION = 7200;
  static constexpr size_t REQUEST_SIZE = 28, HEADER_SIZE = 64, RECORD_SIZE = 76;
  struct Record {
    int64_t sum = 0;
    uint32_t count = 0, last = 0, seq = 0, window = 0, utc = 0;
    int8_t minimum = 0, maximum = 0;
    uint8_t flags = 0;
  };
  struct Slot {
    uint8_t key[32] = {};
    bool used = false, active = false;
    uint32_t heard = 0;
    Record current, history[HISTORY];
  };
private:
  Slot slots[IDENTITIES];
  uint8_t session[16] = {};
  uint64_t elapsed = 0;
  uint32_t lastMillis = 0, cleanupAt = 0, period = 0, serial = 0;
  uint32_t expired = 0, evicted = 0, dropped = 0;
  uint32_t anchor = 0;
  bool utcValid = false, changedClock = false, sessionReady = false;
  static void inc(uint32_t& v) { if (v != UINT32_MAX) ++v; }
  static void put(uint8_t* p, uint64_t v, unsigned n) {
    for (unsigned i=0;i<n;++i) { p[i]=uint8_t(v); v >>= 8; }
  }
  static uint32_t get(const uint8_t* p) {
    return uint32_t(p[0]) | uint32_t(p[1])<<8 | uint32_t(p[2])<<16 | uint32_t(p[3])<<24;
  }
  static bool validUTC(uint32_t t) { return t >= 1577836800UL && t < 4102444800UL; }
  void complete(Slot& slot) {
    auto& r=slot.current;
    if (!r.count) return;
    if (!sessionReady || serial==UINT32_MAX) { inc(dropped); r=Record(); return; }
    unsigned dest=0;
    for (unsigned i=0;i<HISTORY;++i) {
      if (!slot.history[i].count) { dest=i; break; }
      if (slot.history[i].seq < slot.history[dest].seq) dest=i;
    }
    if (slot.history[dest].count) inc(evicted);
    r.seq=++serial; r.window=period;
    r.utc=utcValid && !changedClock ? anchor : 0;
    r.flags |= r.utc ? 1 : 0;
    if (changedClock) r.flags |= 2;
    slot.history[dest]=r; r=Record();
  }
public:
  void begin(uint32_t ms, uint32_t rtc, const uint8_t boot[16], bool validSession = true) {
    // Called once at startup; avoids creating a large temporary on the stack.
    for(auto& s:slots) s=Slot();
    sessionReady=validSession;
    memcpy(session,boot,16); lastMillis=ms; elapsed=0; period=serial=cleanupAt=0;
    expired=evicted=dropped=0; utcValid=validUTC(rtc); anchor=utcValid?rtc:0; changedClock=false;
  }
  uint32_t seconds() const { return uint32_t(elapsed/1000); }
  void tick(uint32_t ms, uint32_t rtc) {
    elapsed += uint32_t(ms-lastMillis); lastMillis=ms;
    uint32_t now=seconds(), next=now/PERIOD;
    bool valid=validUTC(rtc);
    // Never backfill UTC into an already-running invalid or corrected interval.
    int64_t expected=int64_t(anchor)+int64_t(now)-int64_t(period)*PERIOD;
    if (valid != utcValid || (valid && (int64_t(rtc)-expected > 2 || expected-int64_t(rtc) > 2)))
      changedClock=true;
    if (next != period) {
      // Expire by window end, even if the loop skipped multiple periods.
      for (auto& s:slots) for (auto& r:s.history)
        if (r.count && uint64_t(now) >= (uint64_t(r.window)+1)*PERIOD+RETENTION) {r=Record();inc(expired);}
      for (auto& s:slots) {
        if (s.current.count && uint64_t(now) >= (uint64_t(period)+1)*PERIOD+RETENTION) {
          s.current=Record(); inc(expired);
        } else complete(s);
      }
      period=next; utcValid=valid; changedClock=false;
      anchor=valid ? rtc-(now%PERIOD) : 0;
    }
    if (now-cleanupAt >= 60) {
      cleanupAt=now;
      for (auto& s:slots) {
        if(s.active && now-s.heard >= RETENTION) s.active=false;
        bool retained=s.current.count;
        for(auto& r:s.history) {
          if(r.count && uint64_t(now) >= (uint64_t(r.window)+1)*PERIOD+RETENTION) {r=Record();inc(expired);}
          retained |= r.count!=0;
        }
        if(!s.active && !retained) s=Slot();
      }
    }
  }
  bool active(const uint8_t* key) const {
    for(const auto& s:slots) if(s.used && s.active && !memcmp(s.key,key,32)) return true;
    return false;
  }
  uint32_t heard(const uint8_t* key) const {
    for(const auto& s:slots) if(s.used && !memcmp(s.key,key,32)) return s.heard;
    return 0;
  }
  void observe(const uint8_t* key, int8_t snr) {
    Slot* found=nullptr; Slot* free=nullptr; Slot* oldestInactive=nullptr; Slot* oldestActive=nullptr;
    unsigned activeCount=0;
    for(auto& s:slots) {
      if(s.used && !memcmp(s.key,key,32)) found=&s;
      if(!s.used && !free) free=&s;
      if(s.active) { ++activeCount; if(!oldestActive || s.heard<oldestActive->heard) oldestActive=&s; }
      else if(s.used && (!oldestInactive || s.heard<oldestInactive->heard)) oldestInactive=&s;
    }
    if(!found) {
      found=free?free:oldestInactive;
      if(!found) {inc(dropped);return;}
      if(found->used) {
        if(found->current.count) inc(evicted);
        for(const auto& r:found->history) if(r.count) inc(evicted);
      }
      *found=Slot(); found->used=true; memcpy(found->key,key,32);
    }
    if(!found->active && activeCount>=ACTIVE && oldestActive) oldestActive->active=false;
    found->active=true;found->heard=seconds();
    auto& r=found->current;
    if(r.count==UINT32_MAX) {inc(dropped);r.flags|=4;return;}
    if(!r.count) r.minimum=r.maximum=snr;
    if(snr<r.minimum) r.minimum=snr;
    if(snr>r.maximum) r.maximum=snr;
    r.sum+=snr;++r.count;r.last=seconds();
  }
  // Read-only snapshot traversal; maximum one full-key record fits a response.
  size_t reply(const uint8_t* req,size_t len,uint8_t* out,size_t capacity) const {
    if(capacity<HEADER_SIZE) return 0;
    memset(out,0,HEADER_SIZE);out[0]=VERSION;memcpy(out+4,session,16);
    uint8_t error=0;uint32_t snapshot=serial,after=0;
    if(len!=REQUEST_SIZE) error=2;
    else if(req[1]!=VERSION) error=1;
    else if(req[2]>1 || req[3]) error=2;
    else {
      uint8_t zero[16]={};bool initial=!memcmp(req+4,zero,16);
      snapshot=get(req+20);after=get(req+24);
      if(initial) { if(snapshot || after) error=2; snapshot=serial; }
      else if(memcmp(req+4,session,16)) error=3;
      else if(snapshot>serial || after>snapshot) error=2;
    }
    if (!sessionReady) error=5;
    const Record* selected=nullptr;const Slot* owner=nullptr;
    uint32_t oldest=UINT32_MAX,newest=0,oldWindow=UINT32_MAX,newWindow=0;
    unsigned activeCount=0,retained=0;
    for(const auto& s:slots) {
      activeCount+=s.active;
      for(const auto& r:s.history) if(r.count) {
        ++retained;if(r.seq<oldest)oldest=r.seq;if(r.seq>newest)newest=r.seq;
        if(r.window<oldWindow)oldWindow=r.window;
        if(r.window>newWindow)newWindow=r.window;
        if(r.seq>after && r.seq<=snapshot && (!selected || r.seq<selected->seq)){selected=&r;owner=&s;}
      }
    }
    bool data=!error && req[2] && selected && capacity>=HEADER_SIZE+RECORD_SIZE;
    if(!error && req[2] && selected && !data) error=4;
    uint32_t cursor=data?selected->seq:after;
    out[1]=error;out[2]=data?1:0;
    if(!error) for(const auto& s:slots) for(const auto& r:s.history)
      if(r.count && r.seq>cursor && r.seq<=snapshot)out[3]=1;
    put(out+20,snapshot,4);put(out+24,cursor,4);
    put(out+28,retained?oldest:0,4);put(out+32,newest,4);
    put(out+36,expired,4);put(out+40,evicted,4);put(out+44,dropped,4);
    put(out+48,activeCount,2);put(out+50,retained,2);put(out+52,IDENTITIES*HISTORY,2);put(out+54,IDENTITIES,2);
    put(out+56,retained?oldWindow:0,4);put(out+60,newWindow,4);
    if(!data)return HEADER_SIZE;
    auto& r=*selected;uint8_t* p=out+HEADER_SIZE;
    memcpy(p,owner->key,32);put(p+32,r.seq,4);put(p+36,r.window,4);
    put(p+40,uint64_t(r.window)*PERIOD,4);put(p+44,(uint64_t(r.window)+1)*PERIOD,4);
    put(p+48,r.utc,4);put(p+52,r.utc?r.utc+PERIOD:0,4);
    put(p+56,uint64_t(r.sum),8);put(p+64,r.count,4);
    p[68]=uint8_t(r.minimum);p[69]=uint8_t(r.maximum);p[70]=r.flags;p[71]=0;put(p+72,r.last,4);
    return HEADER_SIZE+RECORD_SIZE;
  }
};
static_assert(sizeof(NeighbourSNR) <= 24000, "Review XIAO aggregation RAM budget");
