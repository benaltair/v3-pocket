#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <Packet.h>
#include <helpers/ui/MomentaryButton.h>
#include "UITask.h"

static uint32_t now = 100;
static int level = HIGH;
uint32_t millis() { return now; }
unsigned graphClockReads = 0;
int64_t esp_timer_get_time() { ++graphClockReads; return int64_t(now) * 1000; }
int digitalRead(int pin) { assert(pin==0); return level; }
MomentaryButton user_btn(0,1000,true,false,false);
ColorVal UIColor::primary_txt=1, UIColor::window_bkg=0;
struct TestBoard : mesh::MainBoard {
  uint16_t volts=3810;
  unsigned reads=0;
  bool off=false;
  uint16_t getBattMilliVolts() override { ++reads; return volts; }
  const char* getManufacturerName() const override { return "Heltec V3"; }
  uint8_t getStartupReason() const override { return 0; }
  void reboot() override { assert(false); }
  void powerOff() override { off=true; }
} board;
struct Driver {
  uint32_t rx = 26, tx = 19;
  unsigned rxReads = 0;
  uint32_t getPacketsRecv() { ++rxReads; return rx; }
  uint32_t getPacketsSent() { return tx; }
} radio_driver;
mesh::Packet::Packet() : header(0), payload_len(0), path_len(0) {}
struct MyMesh {
  struct {uint8_t pub_key[32]={0xfe};} self_id;
  struct Rng { void random(uint8_t* buf, size_t size) { memset(buf,0x42,size); } } rng;
  Rng* getRNG() { return &rng; }
  bool busy=false;
  int transmissions=0, canceled=0;
  mesh::Packet tx;
  mesh::Packet* createControlData(const uint8_t* bytes,size_t size) {
    if(busy) return nullptr;
    memcpy(tx.payload,bytes,size); tx.payload_len=size;
    tx.header=PAYLOAD_TYPE_CONTROL<<PH_TYPE_SHIFT;
    return &tx;
  }
  void sendZeroHop(mesh::Packet* p) { assert(p==&tx); ++transmissions; }
  void cancelPocketRequest(const mesh::Packet* p,const uint8_t* tag) {
    assert(p==&tx && memcmp(tag,tx.payload+2,4)==0); ++canceled;
  }
} the_mesh;
#define ADV_TYPE_REPEATER 2
#include <PocketRepeaterUI.h>

struct Screen : DisplayDriver {
  bool on=false; int x=0,y=0,size=1; std::vector<std::string> lines,draws;
  Screen():DisplayDriver(128,64){}
  bool isOn() override{return on;}
  void turnOn() override{on=true;}
  void turnOff() override{on=false;}
  void clear() override{}
  void startFrame(ColorVal) override {lines.clear();draws.clear();}
  void setColor(ColorVal) override{}
  void setTextSize(int sz) override{size=sz;}
  void setCursor(int a,int b) override{x=a;y=b;}
  void print(const char* s) override{
    assert(x>=0 && x+6*size*(int)strlen(s)<=128 && y>=0 && y+size*8<=64);
    lines.emplace_back(s);char buf[128];snprintf(buf,sizeof(buf),"T %d %d %d %s",x,y,size,s);draws.emplace_back(buf);
  }
  void fillRect(int x,int y,int w,int h) override {
    assert(x>=0 && y>=0 && x+w<=128 && y+h<=64);
    char buf[80];snprintf(buf,sizeof(buf),"R %d %d %d %d",x,y,w,h);draws.emplace_back(buf);
  }
  void drawRect(int x,int y,int w,int h) override {fillRect(x,y,w,1);fillRect(x,y+h-1,w,1);fillRect(x,y,1,h);fillRect(x+w-1,y,1,h);}
  void drawXbm(int,int,const uint8_t*,int,int) override{}
  uint16_t getTextWidth(const char* s) override{return 6*size*strlen(s);}
  void endFrame() override{}
  bool has(const char* s) {return std::find(lines.begin(),lines.end(),s)!=lines.end();}
  void frame(const char* page){printf("PAGE %s\n",page);for(const auto& line:draws) puts(line.c_str());}
};
static void testVoltage() {
  TestBoard input;
  PocketVoltage filter;
  uint32_t tick = UINT32_MAX - 2500; // Cross the uptime rollover.
  input.volts = 3770;
  for (int n = 0; n < 8; ++n, tick += 1000) {
    assert(filter.read(input, tick) == 3770);
    assert(filter.read(input, tick + 900) == 3770);
  }
  assert(input.reads == 8);
  input.volts = 4200;
  assert(filter.read(input, tick) == 3770); // One spike has no effect on a settled window.
  tick += 1000;
  unsigned last = 3770;
  for (int n = 0; n < 7; ++n, tick += 1000) {
    const unsigned mv = filter.read(input, tick);
    assert(mv >= last && mv <= 4200);
    last = mv;
  }
  assert(last == 4200);
  input.volts = 0;
  assert(filter.read(input, tick) == 4200);
  assert(filter.read(input, tick + 1000) == 4200);
  assert(filter.read(input, tick + 2000) == 0);
  input.volts = 3660;
  assert(filter.read(input, tick + 3000) == 3660);
}
static void testNames() {
  char out[33];
  auto check = [&](const char* input, const char* expected) {
    PocketName::format(out,input,strlen(input),16);
    assert(strcmp(out,expected)==0);
  };
  check("Hilltop 📻","Hilltop");
  check(" A 👩🏽‍🔧 🇨🇦 B ", "A B");
  check("1️⃣Ridge📻", "Ridge");
  check("🦅",""); check("Café", "Caf\x82");
  check("ABCDEFGHIJKLMNOPQ📻","ABCDEFGHIJKLM...");
  check("A\xF0\x9F" "B", "AB");
  const char bounded[] = {'A',char(0xF0),char(0x9F)};
  PocketName::format(out,bounded,sizeof(bounded),16);
  assert(strcmp(out,"A")==0);
}
static void testTraffic() {
  Screen screen; PocketTraffic graph;
  uint32_t count=0;
  auto draw=[&](uint64_t t) {screen.startFrame(0);graph.draw(screen,t,count);};
  auto rect=[&](const char* r) {return std::find(screen.draws.begin(),screen.draws.end(),r)!=screen.draws.end();};
  uint64_t t=4294970; // Beyond the 32-bit millis rollover.
  draw(t);assert(screen.has("Nothing heard"));
  // An empty plot contains just its two labels. Every drawn rectangle represents traffic.
  assert(screen.draws.size() == 2 && screen.lines.size() == 2);
  ++count;draw(t);++count;draw(t+9);
  assert(screen.has("Last heard now") && rect("R 121 33 3 8"));
  assert(screen.draws.size() == screen.lines.size() + 1);
  draw(t+10);assert(screen.has("Last heard 1s") && rect("R 117 33 3 8"));
  ++count;draw(t+11);assert(rect("R 121 37 3 4"));
  draw(t+300);assert(screen.has("Last heard 4m") && rect("R 5 37 3 4") && !rect("R 5 33 3 8"));
  draw(t+310);assert(!rect("R 5 37 3 4") && !rect("R 121 37 3 4"));
  assert(screen.draws.size() == screen.lines.size());
  draw(t+7211);assert(screen.has("Last heard 2h"));
  draw(t+86411);assert(screen.has("Last heard 1d"));
  count+=100;
  draw(t+86411);assert(rect("R 121 25 3 16"));
}
int main(int argc, char** argv){
  testVoltage();testNames();testTraffic();
  Screen screen; NodePrefs prefs={"Hilltop 📻"}; UITask task(board,screen);
  auto step=[&](unsigned ms){now+=ms;task.loop();};
  auto tap=[&](){level=LOW;step(1);level=HIGH;step(40);};
  auto hold=[&](){level=LOW;step(1);step(1000);level=HIGH;step(1);};
  if (argc == 2 && strcmp(argv[1], "--traffic-preview") == 0) {
    radio_driver.rx=0;
    task.begin(&prefs,"","");step(1);screen.frame("quiet");
    const unsigned burst[]={0,0,1,3,2,0,0,0,1,0,0,4,2,0,1,1,0,0,0,0,1,4,8,5,3,2,0,1,0};
    for (unsigned count: burst) {
        tap();tap();    // Keep Home visible between samples.
        radio_driver.rx+=count;
        step(10000);
    }
    assert(screen.on);screen.frame("busy");
    return 0;
  }
  task.begin(&prefs,"","");step(1);assert(screen.has("Hilltop") && screen.has("3.81 V") && screen.has("RX 26") && screen.has("TX 19"));assert(screen.has("RX 5m") && screen.has("Last RX --"));
  assert(strcmp(prefs.node_name, "Hilltop 📻")==0);
  for (int i=0;i<7;++i) step(1000);
  board.volts=3300;step(1000);
  assert(screen.has("3.81 V") && std::find(screen.draws.begin(),screen.draws.end(),"R 115 5 2 4")!=screen.draws.end());
  board.volts=3810;
  step(30000);assert(!screen.on);
  const unsigned burstCounts[] = {1,0,2,1,0,3,1,1,0,0,2,4,2,0,1,0,1,3,0,2,1,0,5,3,1,2,0,3,2};
  auto readsWhileDark = board.reads;
  const auto clockBefore = graphClockReads, rxBefore = radio_driver.rxReads;
  for (unsigned count : burstCounts) {
    step(10000);
    radio_driver.rx += count;
    assert(!screen.on);
  }
  assert(clockBefore==graphClockReads && rxBefore==radio_driver.rxReads);
  assert(readsWhileDark==board.reads && the_mesh.transmissions==0);
  step(11000);hold();assert(screen.on && screen.has("Hilltop") && !board.off);
  assert(screen.has("Last RX --"));
  radio_driver.rx+=3;step(1000);assert(screen.has("Last heard now"));screen.frame("home");
  radio_driver.tx=20;step(1000);assert(screen.has("Last heard 1s"));
  radio_driver.rx=0;step(1000);assert(screen.has("Nothing heard"));
  tap();assert(screen.has("Repeaters") && screen.has("Hold to discover"));screen.frame("ready");
  uint8_t iconOnly[32]={0xbd};pocketHeard(iconOnly,"📻",2);step(1000);assert(screen.has("BD000000"));
  uint8_t known[32]={0xa2};pocketHeard(known,"Ridge 🦅",12);step(1000);assert(screen.has("Ridge") && screen.has("3.00 dB"));
  tap();tap();assert(the_mesh.transmissions==0);
  hold();assert(the_mesh.transmissions==1 && screen.has("Request queued"));screen.frame("queued");
  step(28000);assert(screen.on && screen.has("Request queued"));hold();assert(the_mesh.transmissions==1);
  pocketTxDone(&the_mesh.tx,true);step(1000);assert(screen.has("Listening..."));screen.frame("listening");
  auto reply=[&](uint8_t key,int8_t snr){mesh::Packet p;p.header=(PAYLOAD_TYPE_CONTROL<<PH_TYPE_SHIFT)|ROUTE_TYPE_DIRECT;p.payload_len=38;p.payload[0]=0x92;p.payload[1]=100;memcpy(p.payload+2,the_mesh.tx.payload+2,4);memset(p.payload+6,0,32);p.payload[6]=key;p._snr=snr;return p;};
  auto wrong=reply(0xaa,100);wrong.payload[2]++;pocketReceive(wrong);
  wrong=reply(0xab,99);wrong.path_len=1;pocketReceive(wrong);
  wrong=reply(0xac,98);wrong.payload_len=14;pocketReceive(wrong);
  for(int i=1;i<=5;i++) pocketReceive(reply(i,-i));
  pocketReceive(reply(0xa2,10));pocketReceive(reply(0xa2,-20));
  // Same prefix, distinct full identities. The stronger of two separate entries survives.
  auto shared=reply(1,9);shared.payload[37]=1;pocketReceive(shared);
  step(1000);assert(screen.has("2.25 dB"));assert(screen.lines[1]=="Ridge" && screen.lines[2]=="2.50 dB" && screen.has("01000000") && !screen.has("05000000"));assert(screen.lines.size() >= 10 && screen.lines[8]=="-0.50 dB");screen.frame("results");
  step(13000);assert(screen.on && screen.has("Scan again in 15s"));screen.frame("cooldown");
  step(29999);assert(screen.on);step(1);assert(!screen.on);
  tap();tap();hold();pocketTxDone(&the_mesh.tx,false);step(1000);assert(screen.has("Transmit failed"));
  step(2000);hold();step(30000);assert(the_mesh.canceled==1 && screen.has("TX unconfirmed"));step(2000);
  the_mesh.busy=true;hold();assert(screen.has("Radio busy"));step(2000);the_mesh.busy=false;hold();
  pocketTxDone(&the_mesh.tx,true);step(15000);assert(screen.has("No repeaters heard"));
  tap();hold();step(3000);assert(board.off);
}
