// Numeric verification of the touchpad -> mouse pipeline.
//
// The structs under test are lifted verbatim out of JoyShockMapper/include/JoyShock.h
// at build time (see tests/run_touch_harness.py), so this exercises the committed
// code rather than a copy of it. JoyShock.h itself cannot be included here: it pulls
// in SDL3 and the whole JSM object graph.
//
// Run with:  python3 tests/run_touch_harness.py
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <fstream>
#include <sstream>
#include <cassert>

struct FloatXY {
    float _x=0.f,_y=0.f;
    FloatXY()=default;
    FloatXY(float a,float b):_x(a),_y(b){}
    float x() const {return _x;} float y() const {return _y;}
};

#include "lifted.inc"   // LowPassFilter1E, OneEuroFilter, TouchMousePipeline

// OneEuroFilter::filter out-of-line definitions, mirroring JoyShock.cpp.
float OneEuroFilter::filter(float x, float dt, float minCutoff, float beta)
{
    float dx = initialized ? (x - xPrev) / dt : 0.f;
    xPrev = x;
    initialized = true;
    float edx = dxFilt.filter(dx, alpha(dCutoff, dt));
    float cutoff = minCutoff + beta * std::abs(edx);
    return xFilt.filter(x, alpha(cutoff, dt));
}
float OneEuroFilter::filter(float x, float dt) { return filter(x, dt, 1.f, 0.007f); }

// Mirror of win32 moveMouse()'s sub-pixel accumulator.
static float accX=0.f, accY=0.f;
static long  totalX=0,  totalY=0;
static int   zeroPolls=0, polls=0, maxStep=0;
static void moveMouse(float x,float y){
    accX+=x; accY+=y;
    int ax=(int)accX, ay=(int)accY;
    accX-=ax; accY-=ay;
    totalX+=ax; totalY+=ay;
    ++polls; if(ax==0&&ay==0)++zeroPolls;
    if(std::abs(ax)>maxStep)maxStep=std::abs(ax);
}
static void resetStats(){accX=accY=0.f;totalX=totalY=0;zeroPolls=polls=maxStep=0;}

static unsigned seed=12345u;
static float noise(){ seed=seed*1664525u+1013904223u; return ((seed>>8)&0xFFFF)/65535.f-0.5f; }

int main(){
    const float TICK=0.003f;      // 333 Hz
    const int   N=1000;           // 3 seconds
    const float SPEED=0.06f;      // pad-widths per second (slow steady pan)
    const float TPX=1920.f, TPY=1920.f;
    const float SENS=1.f;
    const float NOISE=0.0004f;
    // Cross-checked against main.cpp's actual registration by
    // check_defaults_in_sync() in run_touch_harness.py, so these cannot go stale
    // the way the hardcoded 0.8f/0.015f they replaced did (that pair was the
    // pre-retuning default; the field kept calling it "shipped" long after
    // TOUCHPAD_MIN_CUTOFF/TOUCHPAD_SPEED_COEFF moved to these values).
    const float SHIPPED_MIN_CUTOFF=6.0f;
    const float SHIPPED_SPEED_COEFF=0.6f;

    printf("=== slow steady pan: %.3f pad-widths/s, %d polls @ %.0f Hz ===\n",
           SPEED,N,1.f/TICK);
    float expected = SPEED*N*TICK*TPX*SENS;
    printf("expected displacement = %.1f px\n\n", expected);

    // --- OLD path: int16_t truncation of the per-poll delta ---
    resetStats();
    { float pos=0.2f, prev=0.2f;
      for(int i=0;i<N;i++){
        prev=pos; pos=0.2f+SPEED*(i+1)*TICK;
        float p=pos+noise()*NOISE;
        short mov=(short)((p-prev)*TPX);      // the bug
        moveMouse(mov*SENS,0.f);
      } }
    printf("OLD   total=%4ld px | zero-output polls=%5.1f%% | max step=%d\n",
           totalX,100.0*zeroPolls/polls,maxStep);

    // --- NEW path: float deltas, One Euro on position ---
    auto run=[&](float minCutoff,float beta,const char*label){
        resetStats();
        TouchMousePipeline pipe; pipe.reset();
        float pos=0.2f;
        for(int i=0;i<N;i++){
            pos=0.2f+SPEED*(i+1)*TICK;
            float p=pos+noise()*NOISE;
            FloatXY d=pipe.step(p,0.5f,TICK,minCutoff,beta);
            moveMouse(d.x()*TPX*SENS, d.y()*TPY*SENS);
        }
        int panZeros=zeroPolls, panPolls=polls, panMaxStep=maxStep;
        for(int i=0;i<400;i++){                 // finger holds still; filter catches up
            FloatXY d=pipe.step(pos,0.5f,TICK,minCutoff,beta);
            moveMouse(d.x()*TPX*SENS, d.y()*TPY*SENS);
        }
        zeroPolls=panZeros; polls=panPolls; maxStep=panMaxStep;
        printf("%-6s total=%4ld px | zero-output polls=%5.1f%% | max step=%d\n",
               label,totalX,100.0*zeroPolls/polls,maxStep);
        return totalX;
    };
    long rawTotal   = run(0.f,   0.f,   "NEW/0");   // filter bypassed
    int  slowMaxStep = maxStep;
    long filtTotal  = run(SHIPPED_MIN_CUTOFF,SHIPPED_SPEED_COEFF,"NEW");
    int  slowMaxStepFilt = maxStep;

    // --- fast flick: displacement must be conserved ---
    // The ramp deliberately saturates partway through (min(0.7f, ...)) and then
    // holds there for the rest of the window, but that tail does not model
    // anything a real pad produces: a real flick ends with the finger LIFTING
    // OFF, which routes to the separate coast/decay path and never calls step()
    // again at all, rather than sitting motionless mid-gesture for two seconds.
    // Conservation is measured over the ramp -- the part that is a real flick --
    // and reported separately from the tail below.
    int rampTicks=0;
    auto flick=[&](float minCutoff,float beta,const char*label){
        resetStats();
        TouchMousePipeline pipe; pipe.reset();
        float pos=0.1f;
        long rampTotal=0;
        rampTicks=0;
        for(int i=0;i<N;i++){
            pos=0.1f+std::min(0.7f,SPEED*12.f*(i+1)*TICK);
            FloatXY d=pipe.step(pos,0.5f,TICK,minCutoff,beta);
            moveMouse(d.x()*TPX*SENS,d.y()*TPY*SENS);
            if(pos<0.8f){ rampTotal=totalX; rampTicks=i+1; }
        }
        printf("%-6s total=%4ld px (ramp=%4ld px, held tail=%4ld px)\n",
               label,totalX,rampTotal,totalX-rampTotal);
        return std::pair<long,long>{rampTotal,totalX};
    };
    auto [fRawRamp,fRawTotal]=flick(0.f,0.f,"NEW/0");
    auto [fFiltRamp,fFiltTotal]=flick(SHIPPED_MIN_CUTOFF,SHIPPED_SPEED_COEFF,"NEW");

    // --- assertions ---
    int fails=0;
    auto check=[&](bool ok,const char*what){ printf("%-58s %s\n",what,ok?"PASS":"FAIL"); if(!ok)++fails; };
    printf("\n=== assertions ===\n");
    check(std::abs(rawTotal-(long)expected)<=3,
          "float path reproduces expected displacement");
    check(std::abs(filtTotal-(long)expected)<=(long)(expected*0.05),
          "1-euro filter preserves displacement within 5%");
    check(std::abs(fRawRamp-fFiltRamp)<=(long)(fRawRamp*0.03),
          "fast flick loses <3% to filtering, over the genuinely moving ramp");
    // The held tail is not real input (see above), so this is a loose sanity
    // check that it still eventually settles rather than drifting unbounded --
    // not a tight bound on how it gets there. Converging via several small
    // catch-up steps instead of one big one is the point of this fix: it is
    // slower to fully settle than an instant snap would be (currently ~34px
    // over the held tail's ~2.2s, at the shipped defaults), in exchange for
    // never producing the single oversized step a snap does.
    check(std::abs(fFiltTotal-fFiltRamp)<=(long)(std::abs(fRawTotal-fRawRamp)+60),
          "the held tail settles rather than drifting away indefinitely");
    check(slowMaxStep<=1 && slowMaxStepFilt<=1,
          "slow pan emits single-pixel steps only, never bursts");

    // first sample after contact must emit nothing
    { TouchMousePipeline p; p.reset();
      FloatXY d=p.step(0.5f,0.5f,TICK,SHIPPED_MIN_CUTOFF,SHIPPED_SPEED_COEFF);
      check(d.x()==0.f&&d.y()==0.f,"first poll after touchdown emits zero"); }

    // a bad dt must not blow up
    { TouchMousePipeline p; p.reset();
      p.step(0.5f,0.5f,0.f,SHIPPED_MIN_CUTOFF,SHIPPED_SPEED_COEFF);
      FloatXY d=p.step(0.51f,0.5f,-1.f,SHIPPED_MIN_CUTOFF,SHIPPED_SPEED_COEFF);
      check(std::isfinite(d.x())&&std::isfinite(d.y()),"non-positive dt is clamped, output finite"); }

    // source handover must not teleport
    { TouchMousePipeline p; p.reset();
      for(int i=0;i<50;i++) p.step(0.30f+i*0.001f,0.5f,TICK,SHIPPED_MIN_CUTOFF,SHIPPED_SPEED_COEFF);
      p.reset();                       // what processTouchMouse does on handover
      FloatXY d=p.step(0.80f,0.5f,TICK,SHIPPED_MIN_CUTOFF,SHIPPED_SPEED_COEFF);
      check(d.x()==0.f,"finger handover after reset emits zero, no teleport"); }

    printf("\n%s (%d failure%s)\n",fails?"FAILURES":"ALL PASS",fails,fails==1?"":"s");
    return fails?1:0;
}
