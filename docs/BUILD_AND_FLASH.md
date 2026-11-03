# 构建、调试与用户自行上板

## 本机实际环境

工程：`D:\STM32Projects\ActiveVisionTracking`。使用已有STM32 bundle的ARM GCC 14.3.1+st.2、CMake4.4.0+st.1、Ninja1.13.2+st.1，不切换MSVC。`tools/build.ps1`自动设置本进程PATH，不修改系统环境变量。

```powershell
cd D:\STM32Projects\ActiveVisionTracking
powershell -ExecutionPolicy Bypass -File tools\build.ps1 -Config Debug -Clean
```

产出：`build\Debug\ActiveVisionTracking.elf/.bin/.hex/.map`。构建日志在`build\verification`。Flash/RAM以链接器真实输出为准，见`IMPLEMENTATION_STATUS.md`。本次基线原工程为Flash5540B、RAM1608B；新增功能全部是User层模块，HAL/CMSIS/链接脚本保持原样。

## 主机软件

本机交付时会配置项目独立`.venv`；首选直接运行：

```powershell
.\.venv\Scripts\python.exe -m host --headless --frames 600 --output runs\demo
.\.venv\Scripts\python.exe -m host --frames 1800 --output runs\gui
.\.venv\Scripts\python.exe -m pytest tests -q
```

重新安装时：`powershell -ExecutionPolicy Bypass -File tools\setup_host.ps1 -Python <可用Python3.11+路径>`。依赖仅NumPy、OpenCV、pyserial、Pillow、pytest。Windows原生C仿真用现有`gcc`（本机Strawberry C），与ARM交叉编译器用途不同。`build/native/avt_sim.dll`是C核心的原生构建；它不能烧录，只有ARM ELF/BIN/HEX用于STM32。中文点阵已生成，无需Pillow参与MCU构建；PC中文显示读取本机微软雅黑字体。

## CubeMX再生成

保留原`.ioc`、72MHz RCC、PE4、PB5、SWD、USER CODE边界。现在TIM3/USART1/FSMC由`User/Src/board_io.c`和`lcd_bus.c`初始化；`.ioc`仍描述原始CubeMX部分，**不能把它当成已包含全部新外设的图形配置**。未用不经验证的手工`.ioc`键值冒充CubeMX生成结果。

如果以后要把外设纳入CubeMX图形配置，请按LCD文档的引脚表设置：FSMC NE4/A10/16bit、USART1 PA9/10 115200、TIM3 PSC71/ARR9999；KEY0保持轮询输入，未启用EXTI。不要启用TIM3 PWM或占用PB0作为本屏背光。把初始化归属迁到生成代码时同步删除User中对应初始化/IRQ实现，避免重复定义与两套配置互相覆盖。

再生成后检查：main.c的USER CODE Includes保留`key.h/app.h/avt_runtime.h`；初始化中保留`KEY0_Init(); AVT_Init(); App_Init();`；循环保留按键/App/AVT任务；CMake仍扫描User/Src直接子目录，新font_cn.c自动进入构建。`tools/audit_project.py`可检查基础资源与USER CODE标记，随后必须重编译。没有执行CubeMX再生成，不能声称再生成流程已实测。

## F5使用本次ELF

先使用Ctrl+Shift+B执行`AVT: build current Debug ELF`。在VS Code当前STM32调试配置中确认程序/ELF为`${workspaceFolder}/build/Debug/ActiveVisionTracking.elf`，不是其他工程路径；如果有preLaunchTask字段，设成上述任务标签。不要切换到旧的Release/Keil产物。构建脚本会打印ELF SHA256，可与状态报告对照。

当前原工程没有launch.json，本次没有写入未经核验的调试适配器配置，也没有运行F5。用户自行用现有ST-LINK配置烧录，保持STM32F103ZE、SWD及正确Flash起始地址0x08000000。先完成LCD自检，再检查主界面与按键，最后接USB串口。Codex未执行烧录、擦除、供电切换。

如果黑屏：按`LCD_INTEGRATION.md`的“插接供电→ID→初始化→色块→坐标→文字→HMI→串口”顺序定位；不要直接跳过控制器ID判断。调试观察`lcd_diag`、`avt_device`、`avt_parser`、`board_diag`、`avt_tasks`和`avt_max_loop_ms`。
