// Shader Model 5.0+

#define WORKGROUP_SIZE 8

Texture2D<float> inputImage : register(t0);

RWTexture2D<float> outputImage : register(u0);


[numthreads(WORKGROUP_SIZE, WORKGROUP_SIZE, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
    uint2 inputSize;
    inputImage.GetDimensions(inputSize.x, inputSize.y);
    
    uint2 outputSize;
    inputImage.GetDimensions(outputSize.x, outputSize.y);
    
    int2 index = int2(DTid.xy);
    if (index.x >= (int)outputSize.x || index.y >= (int)outputSize.y)
        return;
    
    int2 inputCoord = index * 2;
    
    float depth = 0.0f;
    
    [unroll]
    for(int i = 0;i <= 1;i++)
        for(int j = 0;j <= 1;j++)
        {
            depth = max(depth, inputImage.Load(int3(inputCoord + int2(i, j), 0)).x);
        }
    
    outputImage[index] = depth;
}