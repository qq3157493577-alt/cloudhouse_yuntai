# CLOUDHOUSE 寻光云台

基于 STM32F103C8 的云台，可实现姿态随动和寻光功能，使用 STM32 标准外设库。

## CMake 构建

构建流程为：CMake 生成构建规则，Ninja 调用 ARM GCC 编译，链接器按芯片内存布局生成固件。

### 所需工具

- CMake 3.23 或更高版本。
- Ninja。
- ARM GNU Toolchain，包含 `arm-none-eabi-gcc`、`arm-none-eabi-objcopy` 和 `arm-none-eabi-size`。

将这些工具所在的 `bin` 目录加入 PATH 后，新开终端检查：

```powershell
cmake --version
ninja --version
arm-none-eabi-gcc --version
```

STM32Cube 提供的 GNU Tools for STM32 也可以使用。构建配置通过 PATH 查找工具，不依赖个人安装路径。

### 编译命令

在项目根目录执行：

```powershell
cmake --preset debug
cmake --build --preset debug --parallel
```

Release 使用体积优化：

```powershell
cmake --preset release
cmake --build --preset release --parallel
```

也可以在 VS Code 按 `Ctrl+Shift+B` 执行默认的 Debug 构建任务；通过“终端 → 运行任务”选择 Release 构建。

### 构建产物

Debug 产物位于 `build/Debug`，Release 产物位于 `build/Release`：

| 文件 | 用途 |
| --- | --- |
| `cloudhouse_yuntai.elf` | 包含调试信息的可执行固件 |
| `cloudhouse_yuntai.hex` | 包含烧录地址的 Intel HEX 固件 |
| `cloudhouse_yuntai.bin` | 原始固件，烧录起始地址为 `0x08000000` |
| `cloudhouse_yuntai.map` | 查看函数、变量及内存分布 |
| `compile_commands.json` | 供编辑器读取实际编译参数 |

构建本身不连接开发板，也不执行烧录。烧录后需要实际检查舵机、OLED、MPU6050、按键和串口功能。

## 构建文件与原 Keil 配置的对应关系

| 文件 | 职责 |
| --- | --- |
| `CMakeLists.txt` | 显式列出原 Keil 工程的源文件、头文件路径、宏定义、编译和链接选项 |
| `CMakePresets.json` | 共享 Debug / Release 构建入口 |
| `cmake/arm-none-eabi-gcc.cmake` | 选择交叉编译器，并关闭需要目标板运行环境的编译器链接探测 |
| `linker/STM32F103C8Tx_FLASH.ld` | Flash / RAM 布局、段分配、堆栈边界 |
| `Start/gcc/startup_stm32f103xb.S` | GCC 启动代码、中断向量、全局变量初始化和 main 入口 |
| `Start/gcc/newlib_support.c` | newlib 堆边界与裸机运行时支持 |
| `.vscode/tasks.json` | VS Code 构建快捷任务 |

当前目标是 Cortex-M3，使用软浮点 ABI、`STM32F10X_MD` 和 `USE_STDPERIPH_DRIVER` 宏。
Flash 从 `0x08000000` 开始，共 64 KiB；RAM 从 `0x20000000` 开始，共 20 KiB，没有预留 bootloader 偏移。
时钟沿用 `Start/system_stm32f10x.c` 的现有配置。

GCC 启动文件使用 GNU 汇编语法，原 Keil 启动文件不加入 CMake 构建。
旧 CMSIS 的 GCC 分支已适配裸函数汇编、STREX 寄存器约束以及中断/屏障的编译器内存约束；Keil 使用独立的 ARMCC 分支。
更换芯片时，需要同时检查链接脚本、中断向量、器件宏和系统时钟。

OLED 使用 `%f` 显示角度，因此链接时启用 newlib-nano 的浮点格式化功能并链接数学库。
链接脚本预留 4 KiB 堆和至少 2 KiB 栈，`_sbrk` 禁止堆越界；这不等于已经测量过实际最大栈深度。
UART 输出继续调用应用自己的 `Serial_Printf`，标准 `printf` 没有重定向到串口。

## 添加源文件与仓库管理

新增 `.c` 文件后，在 `CMakeLists.txt` 的 `add_executable` 列表中增加对应路径；新增头文件目录后，更新 `target_include_directories`。
新增源文件不会通过目录通配符自动加入，避免误编译其他启动文件或示例程序。

`build/`、Keil 的 `Objects/`、`Listings/` 和个人配置文件由 `.gitignore` 排除。发布固件时可以将 HEX / BIN 附在 GitHub Release 中。

原 Keil 工程文件 `project.uvprojx` 仍可作为独立构建入口；CMake 不要求迁移到 HAL，也不需要重新生成 CubeMX 工程。

## 迁移验证记录

使用 CMake 4.4.0、Ninja 1.13.2 和 GNU Tools for STM32 GCC 14.3.1 完成了本地构建：

| 配置 | Flash 占用 | RAM 链接占用（含预留堆栈） |
| --- | --- | --- |
| Debug | 45,368 / 65,536 字节 | 12,440 / 20,480 字节 |
| Release | 42,472 / 65,536 字节 | 12,440 / 20,480 字节 |

原工程的 43 个 C 源文件均已纳入 CMake；59 个中断向量与 Keil 启动文件顺序一致，生成固件中的向量地址、初始栈顶及堆栈边界已核对。
以上为构建和固件结构检查，尚未进行开发板烧录与功能测试。

参考：[CMake 交叉编译工具链](https://cmake.org/cmake/help/latest/manual/cmake-toolchains.7.html)、[CMake 预设](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html)、[STM32F103C8 产品资料](https://www.st.com/en/microcontrollers-microprocessors/stm32f103c8.html)。
