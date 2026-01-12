# Tiny SVG Render

该项目为北京大学 2025 秋季学期本科生课程 [可视计算与交互概论](https://vcl.pku.edu.cn/course/vci) (Introduction to Visual Computing and Interaction) 课程大作业。基于课程 [Lab0](https://vcl.pku.edu.cn/course/vci/lab0) 的代码框架开发，实现了一个简易的 SVG 渲染器。

## 1. 功能

- 支持解析常见的 SVG 几何形状，如 Rect, Circle, Ellipse, Line, Polygon 等
- 支持解析 SVG 路径 (Path) 属性
- 支持解析常见的 SVG 变换 (transform) ，如 Translate, Rotate, Scale, Skew, Matrix
- 支持解析常见的样式 (Style) 属性，如 Fill, Stroke, Opacity 等 
- 提供了 SSAA 反走样选项，支持 1-16 倍采样倍数
- 支持导出图片

## 2. 快速开始

### 依赖环境
- C++20
- Xmake，可通过 [Xmake 安装说明](https://xmake.io/zh/guide/quick-start#installation) 进行安装

### 编译步骤

1. 克隆或下载本仓库

2. 进入项目主目录，执行如下命令，你将看到一个交互页面

```bash
xmake
xmake run svg
```

### 交互页面说明