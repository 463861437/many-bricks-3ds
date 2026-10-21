# 构建、验证与打包

本文命令均从仓库根目录运行。成品使用者可直接下载 [Releases](https://github.com/463861437/many-bricks-3ds/releases/latest)，无需配置开发环境。

## 依赖

| 用途 | 工具 |
|---|---|
| 3DS 原生编译 | devkitARM、libctru、devkitPro 的 3DS 工具与规则，GNU Make，POSIX shell |
| CIA 封装 | makerom、bannertool |
| CIA 内容校验 | Python 3、ctrtool、devkitARM 中的 arm-none-eabi-objdump |
| 电脑端测试 | 支持 C++17、ASan、UBSan 的 Clang 或 GCC |
| 预览/重新生成字体与图片 | Python 3、Pillow；生成字体另需 Noto Sans SC 字体文件 |

已验证环境为 macOS arm64，devkitARM GCC **16.1.0**、libctru **2.7.0**、makerom **0.18.4**、bannertool **1.2.0**、ctrtool **1.3.0**。其他工具版本和原生交叉编译平台没有同等实测记录；这不是最低版本兼容性承诺。工具链不包含在仓库中。

### 安装 devkitPro 工具链

按 [devkitPro Getting Started](https://devkitpro.org/wiki/Getting_Started) 与 [devkitPro pacman](https://devkitpro.org/wiki/devkitPro_pacman) 的官方说明，为自己的系统安装包管理器，再安装 `3ds-dev` 组。

macOS/Linux 在已配置 devkitPro pacman 的前提下，通常使用：

```sh
sudo dkp-pacman -S 3ds-dev
```

使用系统 `pacman` 并已添加 devkitPro 软件源的环境，对应命令为 `pacman -S 3ds-dev`，权限按系统配置处理。Windows 应在 devkitPro 提供的 MSYS shell 中运行工程脚本；不要直接把 POSIX 命令粘贴到普通 CMD。具体安装方式以官方文档为准。

`build.sh` 优先使用已有 `DEVKITPRO`，未设置时使用 `/opt/devkitpro`。`DEVKITARM` 默认为 `$DEVKITPRO/devkitARM`。非默认路径示例：

```sh
export DEVKITPRO=/path/to/devkitpro
export DEVKITARM="$DEVKITPRO/devkitARM"
export PATH="$DEVKITPRO/tools/bin:$DEVKITARM/bin:$PATH"
```

替换示例路径，确认下列文件存在：

```text
$DEVKITARM/3ds_rules
$DEVKITARM/bin/arm-none-eabi-g++
$DEVKITPRO/libctru/lib/libctru.a
```

### 安装 CIA 工具

从对应上游选择与电脑操作系统、CPU 架构匹配的版本，解压后放入 `PATH`，或在下文显式指定可执行文件路径：

- [makerom 0.18.4](https://github.com/3DSGuy/Project_CTR/releases/tag/makerom-v0.18.4)：本项目已验证的封装版本。
- [bannertool 1.2.0（diasurgical 维护版本）](https://github.com/diasurgical/bannertool/releases/tag/1.2.0)：制作 HOME 图标与横幅；无适用二进制时按其上游说明构建。
- [Project_CTR / ctrtool](https://github.com/3DSGuy/Project_CTR/releases)：校验 CIA；本项目已验证 1.3.0。

makerom 0.19.0 有[上游记录的版本编码缺陷](https://github.com/3DSGuy/Project_CTR/issues/169)，本项目脚本会拒绝使用。建议先采用已验证的 0.18.4；更换其他版本后必须重新执行 CIA 校验。

## 编译

```sh
git clone https://github.com/463861437/many-bricks-3ds.git
cd many-bricks-3ds
./build.sh -j4
```

主要产物：

| 文件 | 用途 |
|---|---|
| `many-bricks-3ds.3dsx` | Homebrew Launcher 启动文件 |
| `many-bricks-3ds.smdh` | Homebrew Launcher 元数据与图标 |
| `many-bricks-3ds.elf` | 本次构建的带符号 ELF，供打包与调试使用 |
| `build/many-bricks-3ds.map` | 链接映射 |

CIA 打包会先编译，再生成图标和横幅、封装应用：

```sh
./build-cia.sh
```

如果工具不在 `PATH`，使用：

```sh
MAKEROM=/path/to/makerom BANNERTOOL=/path/to/bannertool ./build-cia.sh
```

成功后根目录增加 `many-bricks-3ds.cia` 与 `many-bricks-3ds.map`。1.1.1 的 CIA 版本编码为 `1041`，Title ID 为 `000400000F7A3300`。字体、图标及横幅源图已提交，普通编译不需 Pillow、不访问网络，也不需额外 RomFS 资源。

清理后重新构建：

```sh
./build.sh clean
./build-cia.sh
```

## 电脑端回归测试

```sh
./check.sh
```

核心与挡板检查默认通过 `CXX` 使用 `clang++`，其他检查通过 `HOST_CXX` 使用 `c++`。统一指定本机编译器时同时设置两者：

```sh
CXX=clang++ HOST_CXX=clang++ ./check.sh
# 或：CXX=g++ HOST_CXX=g++ ./check.sh
```

不要将这里的 `CXX` 或 `HOST_CXX` 设置为 ARM 交叉编译器。测试生成本机可执行程序，并在电脑上运行 ASan/UBSan。全部检查包括：

- 核心物理、108 关布局、围墙、连击、道具、球容量与数值稳定性。
- 1.1.1 挡板漏接回归：迟接、两侧斜入、移动路径、时间分片与道具组合。
- 存档格式迁移、双槽损坏恢复、选关和触屏/按键控制。
- 绘制边界与像素结果、音频调用及主循环状态切换。

可单独运行 `tests/check-paddle.sh` 等脚本以缩小问题范围。电脑测试无法代替真机输入手感、声音、帧率或 SD 断电测试，详见 [VALIDATION.md](../VALIDATION.md)。

## CIA 校验

先完成 `./build-cia.sh`，再运行：

```sh
python3 packaging/verify-cia.py --output packaging/VERIFIED-BUILD.json
```

验证器使用 `CTRTOOL` 指定的工具或 `PATH` 中的 `ctrtool`，并通过 `DEVKITARM` 定位 `arm-none-eabi-objdump`。默认 SDK 路径与构建脚本一致；自定义示例：

```sh
CTRTOOL=/path/to/ctrtool DEVKITARM=/path/to/devkitpro/devkitARM     python3 packaging/verify-cia.py --output packaging/VERIFIED-BUILD.json
```

`--output` 在验证成功后原子替换报告，避免失败产生的空文件覆盖旧报告。验证器核对 CIA 内容哈希、ExeFS、ELF LOAD 段、版本、Title ID、图标、DSP 映射及实际链接的 SVC。源码、编译器或二进制变化后，旧报告均不能作为新产物的证明。

该 CIA 为自制软件，Ticket、TMD、AccessDescriptor 不具备任天堂官方签名。工具报告的这些签名失败和内容哈希失败含义不同；内容校验通过不等于官方签名有效。

## 预览与字库

预览需要 Pillow。可以单独创建 Python 环境，避免修改系统 Python：

```sh
python3 -m venv .venv
. .venv/bin/activate
python3 -m pip install Pillow
python3 tests/make-preview.py
```

此命令使用本机 C++ 编译器运行真实游戏与绘图代码，生成 `preview.png`、`screens.png` 和 `walls-preview.png`。需要时用 `HOST_CXX=clang++` 指定编译器。预览主动给予分裂道具、布置菜单和结果状态，不是真机截图；历史对比图 `layout-comparison.png` 不由该命令重新生成。

修改了中文文案或关卡名时，使用自己取得的 Noto Sans SC 字体文件重新生成字库：

```sh
python3 packaging/generate-font.py /path/to/NotoSansSC.ttf
./check.sh
./build-cia.sh
```

字体文件本身不随仓库分发；保留 `packaging/FONT-LICENSE.txt`，检查新字是否完整显示。图标与横幅生成脚本为 `packaging/generate-assets.py`；修改资源后同样需要重新构建、校验和目视检查。

## 打包与发布

验证通过后运行：

```sh
python3 packaging/package-release.py
```

脚本先核对报告中的产物大小和 SHA-256，再在 `dist/` 目录生成源码包、安装包与 `SHA256SUMS` 校验值。安装包包含按 SD 卡目录布置的 CIA、3DSX 和 SMDH；源码包包含构建所需代码、资源及许可。ELF/map 不进入公开包，开发者可通过本地构建生成。打包前确认工作目录只包含准备公开的源码、产物和文档；不要将设备转储、存档或本机配置混入发布包。

建议将 CIA、3DSX、SMDH、源码包、安装包及校验值作为 GitHub Release 附件发布，源码使用对应版本 tag。保留同次构建的 ELF/map 便于定位崩溃；修改版二进制不能沿用旧 ELF。发布说明应区分电脑验证、设备验证和未测试项目，并提供 GPL 对应源码及第三方许可。
