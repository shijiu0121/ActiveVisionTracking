# 实现与验收状态（2026-11-02）

本报告依据本次实际文件、编译、测试与Windows端口枚举。没有执行ST-LINK下载/擦除、硬件供电修改或串口实物联调。

| 模块 | 已实现 | 验证状态 |
|---|---|---|
|原工程保护|保留已有未提交修改；复制原始源码；逐文件基线校验再合入|已实际完成；未reset/clean/删除用户文件|
|DS0/KEY0|PB5/PE4、20ms消抖、1s长按、短按状态转换|原key/led文件未改；真实C源码主机测试通过；历史用户实测不扩大到新固件|
|应用状态|待机/运行/暂停/故障，暂停/无效/超时虚拟量归零|已编译、已主机测试；新固件按键待上板|
|时基/调度|原72MHz、SysTick，TIM3内部100Hz中断计数；有界轮询与分区域刷新|已编译、静态核对；TIM实际中断频率/最长主循环待硬件观测|
|LCD底层|FSMC_NE4/A10 16bit、SSD1963读ID/PLL/面板时序/GRAM/PWM|V2.4厂商图纸已匹配；驱动已编译；V4.7底板逐针连通及实物点亮待确认|
|中文HMI|中文标题、状态、离线/本地模拟、目标当前/预测位置、时延、CRC/RX/TX/故障/TIM/溢出、事件、色块四角自检|使用同一C绘图源码的原生截图验证，96字/标点无缺字；实物效果未测|
|串口|USART1 PA9/10 115200，RX1024/TX512环，中断搬字节与错误计数|已编译；纯软件缺端口/断连回退已测；CH340实机链路未测|
|协议|双端帧结构、CRC、seq/session/TTL、半包粘包与错帧恢复、遥测/RTT|C/Python/标准库固定向量及异常输入测试通过|
|PC算法|像素检测+颜色ROI、卡尔曼速度估计、遮挡预测、丢失安全与重捕获|已实际执行600帧；默认无需摄像头|
|时延模型|延时、抖动、丢包、乱序，未来坐标、补偿开关与同批事件配对误差|已实际执行并生成CSV/JSON/中文截图|
|软件闭环|Python编码→原生C核心→Python解码状态|已实际运行；不能称作STM32硬件联调|
|GUI|中文OpenCV窗口、轨迹、补偿误差、虚拟H/V、后端、Q/P/R|在本机实际运行90帧后自动退出，退出码0|
|触摸/运行时旋转|未启用|实际触摸装配芯片未确认；固定横屏，KEY0满足首版操作|
|真实电机/PWM|未实现、未配置|首版范围明确排除|

## 实际固件构建

最终目录：`D:\STM32Projects\ActiveVisionTracking`。

命令：`powershell -NoProfile -ExecutionPolicy Bypass -File tools\build.ps1 -Config Debug -Clean`。

- ARM GCC 14.3.1，CMake4.4.0，Ninja1.13.2；最终干净构建成功，0警告；User源码开启-Wextra/-Werror。
- ELF：`build/Debug/ActiveVisionTracking.elf`；同时生成BIN/HEX/MAP。
- Flash **26,596 B / 524,288 B = 5.07%**；RAM **3,968 B / 65,536 B = 6.05%**，按链接器区域用量，包含链接脚本的保留区。
- GNU size：text26500、data96、bss3872。中文字符表96项约7104B；没有800x480整帧MCU缓冲。
- ELF SHA256：`4DFF9121973FC25924E663804A1314A0A438309936EBF7FC96E571C2230E7C35`。
- 基线Flash5540B/RAM1608B。`.ioc`、HAL/CMSIS、链接脚本、原KEY0/LED保持不变；Core只在main.c的USER CODE中加初始化/任务入口。USART1/TIM3 IRQ属于User，未启用EXTI。
- 构建日志：`build/verification/build_Debug.log`。

## 实际软件测试

命令：`.venv\Scripts\python.exe -m pytest tests -q --basetemp build\acceptance_tests`。

**22 passed in 12.10s**（该时间为本次记录，不保证每次相同）。包括CRC标准向量与双端帧、随机切片/噪声/坏CRC/超长头恢复、半包超时、重复/乱序/seq回绕、TTL与tick回绕、待机/暂停/故障归零、非法字段、新会话重连、像素跟踪/遮挡恢复/空图失效、缺端口与断开回退、360帧补偿对照、真实key/app/led C代码、真实C中文HMI渲染/边界/缺字检查。

命令：`.venv\Scripts\python.exe -m host --headless --frames 600 --output runs\acceptance`。

- 600帧：TRACKING474、OCCLUDED72、LOST46、REACQUIRING8。
- 模拟时延120ms、抖动±20ms、丢包概率3%；实际模型丢包12，符合固定种子执行结果。
- C核心接收573个有效协议目标包；CRC错误0；11个迟到/乱序包被拒收（对应duplicate计数），不是硬件丢包。
- 522个有效到达事件配对：不补偿平均位置误差**15.3858px**，补偿**5.7035px**，减少约62.9%；图像处理p95约**3.06ms**。
- 这是单一受控合成场景数据，不是通用视觉基准，不是机械跟踪精度。当前颜色跟踪器对复杂场景和同色目标有局限。
- CSV、summary.json、preview.png、occlusion.png位于`runs/acceptance`；测试日志在`build/verification/tests.log`。
- `.venv`独立安装NumPy、OpenCV、Pillow、pyserial、pytest；实际版本记录`build/verification/python_packages.txt`。

## 硬件尚缺的最小确认

1. V4.7底板LCD插座1脚/电源/NE4-A10连通，以及USB串口桥接跳线位号和方向；V3跳线编号没有直接用于V4.7。屏幕本身V2.4图纸已取得，无须再猜控制器。
2. 用户上板反馈：自检颜色/四角/中文是否正确，`lcd_diag.id`是否0x5761；随后真实COM回包是否稳定。

本次Windows实时枚举仅见COM4/5/6/7蓝牙串口，未见当前连接的CH340设备；不指定虚构的COM号，不开启这些蓝牙端口。用户接入板载USB串口后先运行`--list-ports`。

`.ioc`仍是原始CubeMX配置；新外设在User初始化。将其迁回CubeMX并验证再生成属于后续维护事项，详见构建指南，不能宣称已完成图形配置同步。

建议下一步：**先核对V4.7接口，再由用户烧录色块自检**，通过后验收中文HMI/KEY0，最后串口联动。无画面按LCD文档从供电/插接与读ID开始，不从上位机算法排查。
