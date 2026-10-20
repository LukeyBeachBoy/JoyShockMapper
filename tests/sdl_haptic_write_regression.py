from pathlib import Path
import subprocess, tempfile, shutil
ROOT = Path(__file__).resolve().parents[1]
PATCH = ROOT / 'cmake/PatchSdlTritonTouch.cmake'
FIXTURE = '''void touch() {
pTritonReport->sPressureLeft > 0,
pTritonReport->sLeftPadX / 65536.0f + 0.5f,
-(float)pTritonReport->sLeftPadY / 65536.0f + 0.5f,
pTritonReport->sPressureRight > 0,
pTritonReport->sRightPadX / 65536.0f + 0.5f,
-(float)pTritonReport->sRightPadY / 65536.0f + 0.5f,
}
static bool effect(SDL_HIDAPI_Device *device, const void *data, int size) {
    if (size == HID_FEATURE_REPORT_BYTES) {
        return SDL_hid_send_feature_report(device->dev, data, size) == size;
    }
    return SDL_Unsupported();
}
'''
LEGACY = '''    const Uint8 *jsm_bytes = (const Uint8 *)data;
    if (size > 1 && size < HID_FEATURE_REPORT_BYTES &&
        jsm_bytes[0] >= ID_OUT_REPORT_HAPTIC_RUMBLE && jsm_bytes[0] <= ID_OUT_REPORT_HAPTIC_SCRIPT) {
        return SDL_hid_write(device->dev, data, size) == size;
    }
'''
STUB = '''#include <cassert>
#include <cstdio>
using Uint8 = unsigned char;
struct SDL_HIDAPI_Device { void *dev; };
constexpr int HID_FEATURE_REPORT_BYTES=64;
constexpr int ID_OUT_REPORT_HAPTIC_RUMBLE=0x80, ID_OUT_REPORT_HAPTIC_SCRIPT=0x85;
int writeResult, outputCalls=0, featureCalls=0;
int SDL_hid_write(void*,const void*,int) { ++outputCalls; return writeResult; }
int SDL_hid_send_feature_report(void*,const void*,int) { ++featureCalls; return writeResult; }
bool SDL_Unsupported() { return false; }
'''
CHECKS = '''int main() {
 SDL_HIDAPI_Device dev{}; Uint8 data[64]={0x85,5,7,0};
 // Windows HIDAPI returns the descriptor's padded output length, although
 // the firmware has already received and started the four-byte script.
 for(int size : {4,8,12,20}) {
  writeResult=64; assert(effect(&dev,data,size));
  writeResult=size; assert(effect(&dev,data,size));
  for(int failure : {-1,0,size-1}) { writeResult=failure; assert(!effect(&dev,data,size)); }
 }
 const int before=outputCalls;
 data[0]=0x86; writeResult=64; assert(!effect(&dev,data,4)); assert(outputCalls==before);
 data[0]=0x7f; assert(!effect(&dev,data,4)); assert(outputCalls==before);
 // The power-off/settings feature path keeps its existing strict contract.
 data[0]=1; writeResult=64; assert(effect(&dev,data,64)); assert(featureCalls==1);
 writeResult=-1; assert(!effect(&dev,data,64));
 puts("PASS: padded and exact haptic writes succeed; failed, partial and unsupported reports fail; feature reports unchanged");
}
'''
with tempfile.TemporaryDirectory(prefix='jsm-hid-write-') as directory:
 root=Path(directory); driver=root/'src/joystick/hidapi/SDL_hidapi_steam_triton.c'
 driver.parent.mkdir(parents=True)
 script=root/'patch.cmake'
 script.write_text(f'include("{PATCH.as_posix()}")\npatch_sdl_triton_touch("{root.as_posix()}")\n')
 outputs=[]
 for legacy in (False,True):
  driver.write_text(FIXTURE.replace('    if (size == HID_FEATURE_REPORT_BYTES) {', LEGACY+'    if (size == HID_FEATURE_REPORT_BYTES) {') if legacy else FIXTURE)
  subprocess.run(['cmake','-P',str(script)],check=True,capture_output=True)
  outputs.append(driver.read_text())
  subprocess.run(['cmake','-P',str(script)],check=True,capture_output=True)
  assert driver.read_text()==outputs[-1], 'patch must be idempotent'
 assert outputs[0]==outputs[1], 'already-patched SDL must receive the return-value fix too'
 cpp=root/'writes.cpp'; exe=root/'writes.exe'
 cpp.write_text('#include <initializer_list>\n'+STUB+outputs[0][outputs[0].index('static bool effect('):]+CHECKS)
 vcvars=Path('C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Auxiliary/Build/vcvars64.bat')
 batch=root/'build.cmd'
 batch.write_text(f'@echo off\ncall "{vcvars}" >nul\ncl /nologo /EHsc /std:c++17 "{cpp}" /Fe:"{exe}"\n')
 built=subprocess.run(['cmd.exe','/d','/c',str(batch)],cwd=root,capture_output=True,text=True)
 assert built.returncode==0, built.stdout+built.stderr
 run=subprocess.run([str(exe)],capture_output=True,text=True)
 assert run.returncode==0, run.stdout+run.stderr
 print(run.stdout.strip())
