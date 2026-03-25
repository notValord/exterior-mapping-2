/usr/bin/glslc shader.vert -o vert.spv
/usr/bin/glslc shader.frag -o frag.spv 

/usr/bin/glslc frustumShader.vert -o frustumVert.spv
/usr/bin/glslc frustumShader.frag -o frustumFrag.spv 

/usr/bin/glslc lineShader.vert -o lineVert.spv
/usr/bin/glslc lineShader.frag -o lineFrag.spv 

/usr/bin/glslc camCube.vert -o camCubeVert.spv
/usr/bin/glslc camCube.frag -o camCubeFrag.spv 

/usr/bin/glslc offlineShader.vert -o offlineVert.spv
/usr/bin/glslc offlineShader.frag -o offlineFrag.spv 

/usr/bin/glslc rayIntersect.comp -o compute.spv

/usr/bin/glslc getRayData.comp -o rayData.spv
/usr/bin/glslc mipMapReduce.comp -o reduce.spv
/usr/bin/glslc novelSynth.comp --target-env=vulkan1.1 -o novelSynth.spv

/usr/bin/glslc pointCloud.comp -o pointCloud.spv
/usr/bin/glslc pointCloud.vert -o pointVert.spv
/usr/bin/glslc pointCloud.frag -o pointFrag.spv 
