glslc shader.vert -o vert.spv
glslc shader.frag -o frag.spv 

glslc frustumShader.vert -o frustumVert.spv
glslc frustumShader.frag -o frustumFrag.spv 

glslc lineShader.vert -o lineVert.spv
glslc lineShader.frag -o lineFrag.spv 

glslc camCube.vert -o camCubeVert.spv
glslc camCube.frag -o camCubeFrag.spv 

glslc offlineShader.vert -o offlineVert.spv
glslc offlineShader.frag -o offlineFrag.spv 

glslc novelRender.comp -o compute.spv

glslc getRayData.comp -o rayData.spv
glslc mipMapReduce.comp --target-env=vulkan1.1 -o reduce.spv
glslc novelSynth.comp --target-env=vulkan1.1 -o novelSynth.spv
glslc novelReconstruct.comp -o novelReconstruct.spv

glslc pointCloud.comp -o pointCloud.spv
glslc pointCloud.vert -o pointVert.spv
glslc pointCloud.frag -o pointFrag.spv 
