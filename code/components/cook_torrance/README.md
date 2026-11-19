# Cook-Torrance 插件

按照实验一的组件方式编译为 `components/CookTorrance.dll`，注册名称为
`CookTorrance`。构建 `NRenderer` 会同时更新本插件。

## 使用

1. 在 VS Code 中重新配置 CMake，构建并重新启动 `NRenderer`。
2. 在 Asset 中选择“重置 → 重置全部”，通过“导入 → 场景/模型”导入
   `resource/ray_cast_cornel.scn`。本实验统一使用原有 Cornell 场景。
3. 在 Materials 中选择球体绑定的 `Phong` 材质（Type 保持 `1`），
   按下文补充属性。材质名称与 Render Component 的名称是不同概念。
4. 使用下文的球体近景相机，选择 Render Component → `CookTorrance`，点击 Render。
5. 每次使用同一球体比较，保持几何、相机和光源位置不变。

## 结果比较操作

原有场景文件保持不变，所有实验参数在运行界面中修改，不需要为每种材料创建场景。
材质修改默认只保留在本次运行中；重置或重新导入后需要重设，建议逐组截图记录。
原有 conductors.scn 只有 reflect 属性，当前插件不读取它，本实验无需导入该文件。

球体世界坐标为 `(-100,-228,1100)`，半径为 `50`。近景共同设置：
Position `(-100,-228,800)`，LookAt `(-100,-228,1100)`，Up `(0,1,0)`，
Fov `40`，Aspect `1`，Aperture `0`，Width/Height `800/800`。
Ambient 选择 Constant 并设为 `(0,0,0)`；TopLight 位置固定 `(0,273,1028)`。
每次等待 Render 完成后再操作，不修改 White、Red、Green 以及其他模型。
可另用 Position `(0,0,-3)`、LookAt `(0,0,1028)` 拍全景，但组内比较使用固定近景。

在 Asset → Materials 选择 Phong，通过 Add Property... 填写 Property Name、
Property Type、Value 并 Confirm。已有属性直接编辑，属性名区分大小写。

| 属性 | 类型 | 来源或初始值 |
| --- | --- | --- |
| diffuseColor | RGB | 已有；每组按下文设置 |
| specularColor | RGB | 已有；每组按下文设置 |
| specularEx | Float | 已有；每组按下文设置 |
| ambientColor | RGB | 新增 `(0,0,0)` |
| roughness | Float | 新增 `0.35` |
| metallic | Float | 新增 `1` |
| eta | RGB | 新增铜初值 `(0.238,1.007,1.240)` |
| extinction | RGB | 新增铜初值 `(3.626,2.582,2.393)` |
| ior | Float | 新增 `1.5`，仅介电材料实验使用 |

eta、extinction 必须保持 RGB 类型。现有 RGB 编辑器普通拖动限制在 0 到 1；
大于 1 的数值请 Ctrl+单击数值框、键入、Enter 确认，勿使用色盘调整。

### A. Gouraud 与逐像素 Phong

1. TopLight 的 Intensity 设为 `(0.3,0.3,0.3)`。
2. Phong 材质的 diffuseColor 设为 `(0.15,0.15,0.15)`，ambientColor 为零，
   specularColor 为 `(0.8,0.8,0.8)`，specularEx 为 `32`。
3. 依次使用 PhongGouraud、PhongPerPixel 渲染，分别截图。
4. 仅将 specularEx 改为 `128`，重复两个组件的渲染。
5. 再改为 `512`，重复渲染，共六张图。
6. 比较高光位置、宽度、连续性和三角形插值痕迹。指数越大，高光越窄，
   Gouraud 越可能削弱或遗漏三角形内部的高光；逐像素方法仍受同一几何轮廓限制。

### B. 四种材料的 Phong 与 Cook-Torrance

1. TopLight 的 Intensity 设为 `(0.1,0.1,0.1)`。
2. Phong 材质的 diffuseColor、ambientColor 都设为零，metallic 为 `1`，
   roughness 为 `0.35`，specularEx 为 `64`。这两种宽度参数没有严格的一一对应关系。
3. 按下表逐种填写 eta、extinction、specularColor。每种材料依次使用
   PhongPerPixel、CookTorrance 渲染并截图，共八张图。
4. 每一对图保持几何、光源、相机、分辨率及全部材质属性相同，只切换组件。
   重点观察球体；两个模型对墙面、地面及棱锥的响应也不同，背景亮度不能作为
   球体材质的结论。若高光形成大片纯白，应统一降低强度后重做同一组。

| 材料 | eta (RGB) | extinction (RGB) | specularColor (RGB) |
| --- | --- | --- | --- |
| 锡，中性演示近似 | `(2.160,2.160,2.160)` | `(6.350,6.350,6.350)` | `(0.828,0.828,0.828)` |
| 铜 | `(0.238,1.007,1.240)` | `(3.626,2.582,2.393)` | `(0.935,0.624,0.538)` |
| 碳，热解碳近似 | `(1.651,1.615,1.574)` | `(0.822,0.711,0.596)` | `(0.143,0.120,0.098)` |
| 铁 | `(2.911,2.950,2.585)` | `(3.089,2.932,2.767)` | `(0.531,0.512,0.496)` |

铜、铁、热解碳使用 refractiveindex.info 数据库的
[Cu-Johnson](https://refractiveindex.info/?shelf=main&book=Cu&page=Johnson)、
[Fe-Johnson](https://refractiveindex.info/?shelf=main&book=Fe&page=Johnson)、
[C-Querry-Pyrolytic](https://refractiveindex.info/?shelf=main&book=C&page=Querry-Pyrolytic)，
在 650/550/450 nm 取样，作为 RGB 近似，不等于完整光谱到线性 RGB 的积分。
锡采用 [Sn-Golovashkin-293K](https://refractiveindex.info/?shelf=main&book=Sn&page=Golovashkin-293K)
的 730 nm 数值复制到三个通道，仅作中性外观演示；该数据从 730 nm 起始，
不能将本表的锡参数称为可见光 RGB 实测结果。
碳有多种形态；此处的 metallic=1 仅表示使用复折射率不透明吸收体分支。

specularColor 由同组 eta、extinction 的法向反射率 F0 计算，便于颜色对照。
Phong 忽略 eta、extinction、roughness；CookTorrance 忽略 specularColor、specularEx。
metallic=1 时 CookTorrance 使用 eta，单独修改 ior 不会改变结果。

### C. 粗糙度探究

1. 恢复铜的参数，使用 CookTorrance，仅改变 roughness 为 `0.15/0.35/0.65`，
   同位置分别渲染。比较高光宽度和峰值；若低粗糙度图出现大块纯白，统一降低
   灯光强度，并以该强度重做整组三张图。
2. 保持全部 Phong 参数不变，改用 PhongPerPixel，改变 roughness 后图像应不变，
   因为 Phong 不读取 roughness。不要同时调整 specularEx，否则无法验证此结论。

### D. 消光系数与导体折射率探究

恢复铜的参数和 roughness=0.35，固定 CookTorrance，每次只改变一类属性。

| 消光系数组 | extinction (RGB) | eta |
| --- | --- | --- |
| 0.5 倍 | `(1.813,1.291,1.197)` | 铜初值不变 |
| 1 倍 | `(3.626,2.582,2.393)` | 铜初值不变 |
| 2 倍 | `(7.252,5.164,4.786)` | 铜初值不变 |

渲染三张图，比较反射颜色和强度。此处 extinction 是无量纲消光系数 κ，
与体吸收系数 μ 的关系为 `μ=4*pi*κ/λ`，λ 与 μ 必须使用对应长度单位。
当前实现不模拟体内传播，不能从图片推出穿透距离或体透射率。

然后恢复 extinction 的铜初值，研究导体折射率实部：

| 折射率组 | eta (RGB) | extinction |
| --- | --- | --- |
| 0.5 倍 | `(0.119,0.504,0.620)` | 铜初值不变 |
| 1 倍 | `(0.238,1.007,1.240)` | 铜初值不变 |
| 2 倍 | `(0.476,2.014,2.480)` | 铜初值不变 |

渲染三张图，结合 `F0=((eta-1)^2+κ^2)/((eta+1)^2+κ^2)` 解释变化，
不假设折射率增大必定使反射增强。这些变动样本是参数敏感性实验，不再标为实测铜。
本组无需比较 Phong；若要比较，必须先用每组 eta、κ 重算 specularColor。

### E. 可选：介电材料标量折射率探究

1. 固定 CookTorrance、roughness=0.35，改为 metallic=0、
   diffuseColor=(0.4,0.4,0.4)、ambientColor=0。
2. 仅令 ior 为 `1.1/1.5/2.0`，分别渲染。对应介电 F0 约为
   `0.00227/0.04/0.11111`；eta、extinction 在 metallic=0 时不参与 F0。
3. 本组是介电材料实验，不能将结果标为锡、铜、碳、铁。恢复 metallic=1、
   diffuseColor=0 和铜光学参数后才能继续做导体对比。

### 结果记录

截图标注组件、材料、roughness/specularEx、eta/extinction/ior、灯光强度，
保持同样的显示比例。材质颜色比较使用相同灯光强度；背景不用于判断球体的优劣。
区分 Gouraud 顶点光照插值与 Phong 法线插值，不将共享几何的多边形轮廓归因于 BRDF。
不根据整体亮度直接判断模型正确性；两者高光归一化不同，且当前没有环境反射或
墙面间的间接光照，彩色墙面不会通过间接反射给球体染色。

## 材质属性

| 属性 | 类型 | 默认值 | 含义 |
| --- | --- | --- | --- |
| `diffuseColor` | RGB | `(0.8,0.8,0.8)` | 线性空间漫反射颜色；纯金属不使用漫反射项 |
| `roughness` | Float | `0.35` | 感知粗糙度；有效范围限制为 `[0.045,1]` |
| `metallic` | Float | `0`；旧 type=3 为 `1` | `[0,1]`，0 为介电材料，1 为导体 |
| `ior` | Float | `1.5` | 相对空气的介电材料折射率，用于计算介电材料 F0 |
| `eta` | RGB | `(ior,ior,ior)` | 导体复折射率的实部，各通道可不同 |
| `extinction` | RGB | `(0,0,0)` | 导体复折射率的虚部 κ，无量纲 |
| `ambientColor` | RGB | 与 `diffuseColor` 相同 | 可选的常量环境光近似系数 |

可在已有 Property Editor 中添加上述属性，无需改变导入器。
本插件对所有材质使用 Cook-Torrance 模型，`type=5` 可作为自定义类型；
切换材质 type 本身不会创建属性。`specularEx` 是 Phong 参数，本插件不使用。

若要渲染锡、铜、碳、铁，应查询指定物质/形态和波长下的 η、κ，再填入
`eta`、`extinction`。体吸收系数 μ 的单位为长度的倒数，不能直接作为 κ；
若使用 Beer-Lambert 定义，应按 `κ = μ λ / (4π)` 换算并统一长度单位。
本插件计算表面反射，不模拟物体内部的透射与体吸收。

## 模型与约定

所有方向都由表面指向光源或观察者。使用归一化后的插值法线，计算：

```
alpha = roughness^2
D = alpha^2 / (pi * ((N.H)^2 * (alpha^2 - 1) + 1)^2)
F = F0 + (1 - F0) * (1 - V.H)^5
k = (alpha + 1)^2 / 8
G1(x) = x / (x * (1-k) + k)
G = G1(N.V) * G1(N.L)
specular = D * F * G / (4 * (N.V) * (N.L))
Fv = F0 + (1-F0) * (1-N.V)^5
Fl = F0 + (1-F0) * (1-N.L)^5
diffuse = (1-Fv) * (1-Fl) * (1-metallic) * diffuseColor / pi
Lo_direct += Li * (diffuse + specular) * max(N.L, 0)
```

导体 `F0=((eta-1)^2+κ^2)/((eta+1)^2+κ^2)`，介电材料
`F0=((ior-1)/(ior+1))^2`，中间金属度对两种 F0 做线性混合。
背面入射/观察返回零，对极端输入及分母作有限值处理。
漫反射采用入射、出射两个界面的 Fresnel 衰减，保持互易性并抑制掠射角下的
过量漫反射；这是单次散射近似，不含微表面多次散射补偿。

沿用现有 Phong 插件的模型变换、三角形/BVH 求交、硬阴影、相机、平方根
gamma 和显示截断，因此可在相同几何/灯光下切换 `PhongPerPixel` 作比较。
本实现是直接光照；点光源强度沿用框架约定，不作距离平方衰减；面光源按中心
点近似；常量环境光是 `ambientColor * Ambient` 的经验补光，不是环境贴图积分。
不使用 Depth/Sample Nums，不进行间接光、纹理采样和透射。
纯金属做直接光对比时建议 Ambient 为零，避免经验补光掩盖 Fresnel 效果。
