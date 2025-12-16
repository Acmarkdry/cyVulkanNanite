一个基于Vulkan的Nanite实现方式。
目前还在开发（摸鱼）中，争取2026.3结束。

# 构建

vcpkg_windows.bat拉取第三方库（vcpkg是个好东西，拉取第三方依赖比git external方便很多）。
build_windows.bat进行构建。

# 原理

```mermaid
graph TB
    A[场景数据] --> B[BVH遍历]
    B --> C[视锥剔除]
    C --> D{可见物体}

    D --> E[误差投影]
    E --> F[LOD选择]

    F --> G[硬件光栅化]
    F --> H[软件光栅化]

    G --> I[深度拷贝]
    H --> I
    I --> J[Hi-Z生成]

    J --> K[遮挡剔除]
    K --> L[合并光栅结果]

    L --> M[G-Buffer填充]
    M --> N[着色管线]
    N --> O[天空盒]
    O --> P[后处理]
    P --> Q[最终图像]

    style G fill:#90EE90
    style H fill:#FFB6C1
    style N fill:#87CEEB
```

# 参考资料

[【22.GPU驱动的几何管线-nanite (Part 1) | GAMES104-现代游戏引擎：从入门到实践】](https://www.bilibili.com/video/BV1Et4y1P7ro/?share_source=copy_web&vd_source=de7a08b4d347de57ea41a8ae39a04d3b)
[Epic演讲资料](https://advances.realtimerendering.com/s2021/Karis_Nanite_SIGGRAPH_Advances_2021_final.pdf)
