"""Read-only checks for source ownership and generated integration points."""
from pathlib import Path
import re
ROOT = Path(__file__).resolve().parents[1]
def main():
    errors = []
    for path in (ROOT/'Core').rglob('*.[ch]'):
        text = path.read_text(encoding='utf-8-sig')
        begin = re.findall(r'USER CODE BEGIN ([^*\r\n]+)', text)
        end = re.findall(r'USER CODE END ([^*\r\n]+)', text)
        if sorted(x.strip() for x in begin) != sorted(x.strip() for x in end):
            errors.append(f'USER CODE markers unbalanced: {path}')
    main_c = (ROOT/'Core/Src/main.c').read_text(encoding='utf-8-sig')
    for token in ('AVT_Init();', 'AVT_Task();', 'KEY0_Init();', 'KEY0_GetEvent()', 'App_Init();'):
        if token not in main_c: errors.append(f'main integration missing: {token}')
    ioc = (ROOT/'ActiveVisionTracking.ioc').read_text(encoding='utf-8-sig')
    for token in ('PE4.GPIO_Label=KEY0', 'PB5.GPIO_Label=DS0', 'PA13.Mode=Serial_Wire', 'PA14.Mode=Serial_Wire', 'RCC.SYSCLKFreq_VALUE=72000000'):
        if token not in ioc: errors.append(f'Baseline configuration drift: {token}')
    cmake = (ROOT/'CMakeLists.txt').read_text(encoding='utf-8-sig')
    if 'CONFIGURE_DEPENDS' not in cmake or 'User/Src/*.c' not in cmake: errors.append('User source auto discovery missing')
    if errors: raise SystemExit('\n'.join(errors))
    print('PASS: USER CODE markers, integration calls, baseline GPIO/SWD/clock and User source discovery')
if __name__ == '__main__': main()
