<<<<<<< HEAD
# 面向遮挡与通信时延的主动视觉跟踪系统

**首版是视觉跟踪算法、短时预测与STM32中文监控/虚拟控制显示。无真实电机控制。** 硬件仅使用ATK-DNF103P V4.7战舰底板、ATK-MD0700 V2.4、ST-LINK、Windows PC。

已实现HAL/CMSIS + ARM GCC/CMake固件、SSD1963驱动、中文HMI/色块自检、KEY0/DS0、100Hz TIM3诊断、非阻塞USART1、CRC二进制协议与失联归零；PC具备合成视频图像检测、卡尔曼遮挡预测、时延/抖动/丢包模型、中文可视化、串口与C核心仿真后端、日志/截图。

**固件已编译、主机闭环已测试；LCD点亮、V4.7底板接线与真实串口联调待用户上板。** 屏幕V2.4官方原理图已核实，V4.7底板完整原理图及具体跳线位号尚未获取。详见[实现状态](docs/IMPLEMENTATION_STATUS.md)与[LCD集成证据](docs/LCD_INTEGRATION.md)。

```powershell
cd D:\STM32Projects\ActiveVisionTracking
powershell -ExecutionPolicy Bypass -File tools\build.ps1 -Config Debug
.\.venv\Scripts\python.exe -m host --headless --frames 600 --output runs\headless
.\.venv\Scripts\python.exe -m host --frames 1800 --output runs\gui
.\.venv\Scripts\python.exe -m pytest tests -q
```

首次配置Python：`powershell -ExecutionPolicy Bypass -File tools\setup_host.ps1`。本机交付时已提供可运行`.venv`。默认无需摄像头/串口，仿真后端直接运行原生编译的同一份C协议与业务核心；窗口明确标注软件仿真。P切换补偿、Q退出；串口模式R重试连接。生成`trace.csv/summary.json/preview.png/occlusion.png`。

真实串口：先[核对板载USB串口与跳线](docs/HARDWARE_WIRING.md)，运行`--list-ports`确认COM，再用`--backend serial --port COM实际号`。LCD按KEY0进入运行才会改变虚拟H/V。串口失败回退仿真会明确显示，不能视为硬件联调成功。

单板：短按KEY0循环待机→运行→暂停→运行，长按1秒待机；**按住KEY0再按/释放RESET**进入LCD红绿蓝白黑自检，松开后再短按退出。屏幕无PC时标离线/本地模拟。初次建议先验证色块自检。

| 目录 | 内容 |
|---|---|
|Core / Drivers / cmake / .ioc|保留原CubeMX/HAL工程；新增外设初始化归属User，详见构建文档|
|User/Inc、User/Src|协议、业务安全、板级I/O、LCD、中文字模、UI、调度挂接|
|host|像素级颜色目标检测、卡尔曼跟踪、时延模型、CLI、串口/C仿真|
|tests|C/Python交叉协议、故障保护、按键、真实C中文界面渲染、合成场景验收|
|tools|本机ARM构建、Python环境配置、字模再生成、集成检查|
|docs|LCD资料证据、接线、协议、构建调试、5分钟演示与真实进度|

[构建与用户烧录](docs/BUILD_AND_FLASH.md) · [5分钟演示](docs/DEMO_GUIDE.md) · [协议](docs/PROTOCOL.md)

新增实现没有复制未知许可的第三方LCD驱动或全量字体。已有ST HAL/CMSIS保留原版权与许可；中文点阵由本机已安装字体渲染界面所需字符，仅为本项目生成，不包含完整字体文件。厂商资料链接与适用版本在LCD文档中列明。
=======
STM32学习项目
>>>>>>> 22ebd49f0f79a534298dbfbc38f11aac47209968
