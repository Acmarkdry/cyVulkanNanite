## list

- [x] mesh和lod生成
- [x] cluster and cluster group实现
- [x] bvh traversal
- [x] soft rasterization
- [x] hard rasterization
- [x] hiz
- [ ] Render Graph实现-> 这里打算转为使用vulkan.hpp，重写一套底层，因为很多依赖都要修改，放弃对于vulkan example的依赖。
- [ ] 性能分析
- [ ] mesh多线程处理。
- [ ] 现在渲染出来的是没有颜色的，还有点麻烦，games104课程我记得后面有讲，可以看一下是怎么做的。
- [ ] GPU Driven Depth Culling
- [ ] 更好的内存对齐方式。其实这里我的想法是能不能效仿asan，在最开始的代码中下毒，进行一个padding的验证，而且很麻烦的一点是cpu gpu没有对齐是没有任何warning的，vulkan validation layer对于这个没有任何防御方式
- [ ] GPU上的多线程并发优化
## TODO Fix Bug
- [x] 渲染出来没有几何细节，问题原因：CPU和GPU的内存对齐问题。
- [ ] 软光栅的代码好像没有跑到。
- [ ] bvh和error处理部分的渲染剔除模块有问题，导致在lod切换的时候会出现闪烁。
