# 来源与许可

项目实现、原创关卡、几何图形及合成音效以 GNU GPL v3 或后续版本提供（GPL-3.0-or-later），完整条款见 [LICENSE](LICENSE)。源码工程随发行包提供；构建工具链通过 devkitPro 获取，不嵌入源码包。

- 中文字体：Noto Sans SC，Copyright 2014–2021 Adobe，SIL Open Font License 1.1。生成的字库来自字体栅格化；许可全文见 [FONT-LICENSE.txt](packaging/FONT-LICENSE.txt)。字体源不包含在本包。
- libctru 2.7.0：zlib License，详见 [libctru-LICENSE.txt](packaging/licenses/libctru-LICENSE.txt)。本项目静态链接其系统服务、图形缓冲、输入与 NDSP 支持。
- devkitARM/newlib C 运行时：各组成部分许可见 [newlib-COPYING.txt](packaging/licenses/newlib-COPYING.txt)。
- GCC 运行时：GPL v3 与 GCC Runtime Library Exception 3.1，见 [LICENSE](LICENSE) 和 [GCC-RUNTIME-EXCEPTION.txt](packaging/licenses/GCC-RUNTIME-EXCEPTION.txt)。
- makerom、bannertool、ctrtool 是构建/验证时使用的外部工具，其可执行文件不随本发行包分发。

玩法参考 [FujiCubeSoft 的 Many Bricks Breaker](https://store.steampowered.com/app/1604020/)；此项目没有官方关联。研究中查阅了 huidong 的 [MIT 开源 BricksBeater](https://github.com/zouhuidong/BricksBeater)，未将其代码、关卡数据或其他资源复制到工程。所有包内图片与波形均由本工程脚本/代码生成。
