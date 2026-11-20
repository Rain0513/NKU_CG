# RayTracing 实验插件

完整的构建、界面操作、人工验收和报告说明见 [实验三使用说明](../../../doc/lab3-user-guide.md)。

## 模块

- `RayTracing.dll` 注册 `RayTracingBruteForce` / `RayTracingKDTree`。
- `RayTracingCore` 为两种后端共用的静态算法库。
- `RayTracer` 负责确定性像素采样、直接光照、阴影和递归反射/折射。
- `SceneGeometry` 生成内部世界坐标图元，保留解析球体并展开网格三角形。
- `Intersector` 定义最近命中与任意遮挡接口；`BruteForce`、`KDTree` 分别实现。
- KDTree 使用采样 SAH 选择空间划分，跨界图元保留在两侧；默认叶阈值 4、最大深度 24。根包围盒只求交一次，之后用分割平面、射线区间与显式栈近到远遍历。
- `HitRecord` 保留几何法线、正反面和稳定编号，同距离命中选择更小编号。
- `Optics` 处理折射/全反射及 Schlick Fresnel。
- `RenderStats` 记录时间和可选查询计数。界面关闭计数，只输出单行总时间；检查工具通过 --diagnostics 单独导出计数。

算法源码不依赖窗口；`Adapter` 才负责连接服务器、像素显示及日志。场景是只读输入，方便重复渲染与比较。

## 验证入口

默认不构建额外测试目标。设置 `NR_BUILD_RAY_TRACING_CHECK=ON`，或运行：

```powershell
.\tools\build_ray_tracing.ps1 -WithChecks
.\tools\run_lab3_benchmark.ps1
```

仓库根目录的 `tools` 提供构建和测量脚本；本目录 `tools/RayTracingCheck.cpp` 是唯一保留的回归/测量程序。旧场景生成脚本已删除。

实验独立场景为 `resource/ray_tracing_lab3.scn`，直接复制 `path_tracing_cornel.scn`，仅将原球设为透明玻璃、原长方体设为镜面，原金字塔保留漫反射；参考文件逐字节不变。原先新增的四个场景已删除。

不透明材质可用 ambientColor (RGB) 提供经验填充，UI Ambient Constant 也参与表面填充。此项不是间接光照。新场景保留原顶部面光源、几何形状和所有位置，不新增 ambientColor；透明球、镜面长方体、漫反射金字塔共 16 个几何图元。面积光源用固定 2×2 中点采样近似积分，灯面几何、平面求交系数、采样位置及固定面积权重在初始化时缓存，两种后端共同使用。推荐相机与 --showcase-camera 详见完整说明；测量固定使用默认视点。
