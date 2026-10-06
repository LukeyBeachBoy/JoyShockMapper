"""Native gyro floor regression and a CSV parity fixture for the frontend."""
from pathlib import Path
import os, shutil, subprocess, tempfile
ROOT = Path(__file__).resolve().parents[1]
def balanced(s, signature):
 start=s.index(signature); opening=s.index('{',start); depth=1;end=opening+1
 while depth:
  depth+=(s[end]=='{')-(s[end]=='}');end+=1
 return s[start:end]
source=(ROOT/'JoyShockMapper/src/main.cpp').read_text(encoding='utf-8')
operators=(ROOT/'JoyShockMapper/src/operators.cpp').read_text(encoding='utf-8')
shape=balanced(source,'struct AccelCurveShape')+';'
evaluate=balanced(source,'static float evaluateAccelCurve(')
parser=balanced(operators,'static optional<float> getFloat(')+'\n'+balanced(operators,'istream &operator>>(istream &in, FloatXY &fxy)')
filter_start=source.index('gyro_steadying_floor->setFilter(')
validation=balanced(source[filter_start:],'[](FloatXY current, FloatXY next)')
# Validate wiring and keep the original zero-floor branch in the actual pipeline.
assert 'else if (recovery > speed)' in source
assert 'gyroLength *= gyroIgnoreFactor;' in source
assert 'new JSMSetting<FloatXY>(SettingID::GYRO_STEADYING_FLOOR, { 0.f, 0.f })' in source
curves=['Natural','Power','Quadratic','Sigmoid','Jump']
includes='\n'.join('#include "'+str(ROOT/'JoyShockMapper/src'/f'{c}Curve.cpp').replace('\\','/')+'"' for c in curves)
program=r''' 
#include <algorithm>
#include <cmath>
#include <cassert>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <optional>
#include <string>
using namespace std;
#include "STEADYING_HEADER"
CURVES
struct FloatXY:pair<float,float>{FloatXY(float x=0,float y=0):pair(x,y){}};
PARSER
SHAPE
EVALUATE
void near(float a,float b){if(!isfinite(a)||abs(a-b)>=.0001f){cerr<<"Mismatch "<<a<<" != "<<b<<"\n";abort();}}
float effective(float v,float cutoff,float recovery,float floor,const AccelCurveShape&s,float lo=5,float hi=21){
 const float f=gyroSteadyingFactor(v,cutoff,recovery);
 if(cutoff>0 && (recovery>cutoff?v<=cutoff:v<cutoff))return 0;
 const float curveSpeed=floor>0?v:v*f;
 const float base=evaluateAccelCurve(s,max(0.f,curveSpeed-s.minThreshold),lo,hi);
 return floor>0?gyroSteadyingSensitivity(base,floor,f):f*base;
}
int main(){
 auto validate=VALIDATION;
 FloatXY current(2,1.5f), parsed;
 istringstream one("2");one>>parsed;near(parsed.first,2);near(parsed.second,2);
 istringstream two("2 1.5");two>>parsed;near(parsed.second,1.5f);
 for(float invalid:{-1.f,nanf(""),INFINITY}){auto n=validate(current,FloatXY(invalid,2));near(n.first,2);near(n.second,1.5f);}
 auto valid=validate(current,FloatXY(0,3));near(valid.first,0);near(valid.second,3);
 near(gyroSteadyingSensitivity(5,99,.2f),5);
 near(gyroSteadyingSensitivity(5,-1,.2f),1);
 near(gyroSteadyingSensitivity(5,nanf(""),.2f),1);
 near(gyroSteadyingFactor(1,0,0),1);
 near(gyroSteadyingFactor(1,2,2),0);
 cout<<setprecision(9);
 const float speeds[]={0,.00001f,.1f,1,3,4.99999f,5,5.0001f,20,40,60,80,90,120};
 for(int c=0;c<6;++c){
  AccelCurveShape s;s.curve=static_cast<AccelCurve>(c);s.maxThreshold=80;s.naturalVHalf=20;s.powerVRef=20;s.powerExponent=.5f;s.sigmoidMid=20;s.sigmoidWidth=8;s.jumpTau=1.5f;
  for(float minimum:{0.f,10.f}){s.minThreshold=minimum;
   for(float floor:{0.f,2.f,99.f}) for(float v:speeds){
    float actual=effective(v,0,5,floor,s);
    float base=evaluateAccelCurve(s,max(0.f,v-minimum),5,21);
    assert(actual>=0 && actual<=base+.0001f);
    if(v>=5)near(actual,base);
    if(floor==0){float oldV=v;if(v<5)oldV*=v/5;near(actual,v/5<1?v/5*evaluateAccelCurve(s,max(0.f,oldV-minimum),5,21):base);}
    if(floor==2&&v==0)near(actual,2);
    cout<<c<<","<<minimum<<","<<floor<<","<<v<<","<<actual<<"\n";
   }
   near(effective(4.99999f,0,5,2,s),effective(5,0,5,2,s));
   near(effective(1,2,5,2,s),0);
   near(effective(2,2,5,2,s),0);
   near(effective(3,2,2,2,s),evaluateAccelCurve(s,max(0.f,3-minimum),5,21));
  }
 }
 AccelCurveShape q;q.curve=AccelCurve::QUADRATIC;q.maxThreshold=80;
 near(effective(1,0,5,2,q),2.6005f);near(effective(3,0,5,2,q),3.8135f);
 near(effective(20,0,5,2,q),6);near(effective(40,0,5,2,q),9);
 near(effective(60,0,5,2,q),14);near(effective(80,0,5,2,q),21);
 q.minThreshold=10;near(effective(80,0,5,2,q),17.25f);near(effective(90,0,5,2,q),21);
 near(gyroSteadyingSensitivity(3,1.5f,.2f),1.8f);
 near(gyroSteadyingSensitivity(5,2,.2f),2.6f);
 for(float v:speeds)near(effective(v,0,5,2,q,5,5),2+min(1.f,v/5)*3);
 cerr<<"PASS: native curves, legacy recovery, floor, cutoff, validation, parser, static and asymmetric axes\n";
}
'''
# Curve enum in this fixture follows the six test labels, not enum ordinal assumptions.
program=program.replace('STEADYING_HEADER',str(ROOT/'JoyShockMapper/include/GyroSteadying.h').replace('\\','/')).replace('CURVES',includes).replace('PARSER',parser).replace('SHAPE','enum class AccelCurve { LINEAR,NATURAL,POWER,QUADRATIC,SIGMOID,JUMP,INVALID };\n'+shape).replace('EVALUATE',evaluate).replace('VALIDATION',validation)
compiler = shutil.which('g++') or shutil.which('clang++')
vcvars = Path(os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)')) / 'Microsoft Visual Studio/2022/BuildTools/VC/Auxiliary/Build/vcvars64.bat'
with tempfile.TemporaryDirectory() as folder:
    folder = Path(folder)
    cpp, binary = folder / 'steadying.cpp', folder / ('steadying.exe' if os.name == 'nt' else 'steadying')
    cpp.write_text(program, encoding='utf-8')
    if compiler:
        command = [compiler, '-std=c++17', '-O2', '-I'+str(ROOT/'JoyShockMapper/include'), str(cpp), '-o', str(binary)]
    else:
        batch = folder / 'build.bat'
        batch.write_text(f'@echo off\ncall "{vcvars}" >nul\ncl /nologo /EHsc /std:c++17 /I"{ROOT / 'JoyShockMapper/include'}" "{cpp}" /Fe:"{binary}"\n')
        command = ['cmd.exe', '/d', '/c', str(batch)]
    result = subprocess.run(command, cwd=folder, capture_output=True, text=True)
    if result.returncode:
        print(result.stdout, result.stderr)
        raise SystemExit(result.returncode)
    result = subprocess.run([str(binary)], capture_output=True, text=True)
    fixture = ROOT.parent / 'tmp/gyro-steadying-native.csv'
    fixture.parent.mkdir(parents=True, exist_ok=True)
    fixture.write_text(result.stdout, encoding='utf-8')
    print(result.stderr)
    raise SystemExit(result.returncode)

