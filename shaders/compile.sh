/usr/bin/glslc shader.vert -o vert.spv
/usr/bin/glslc shader.frag -o frag.spv 

/usr/bin/glslc frustumShader.vert -o frustumVert.spv
/usr/bin/glslc frustumShader.frag -o frustumFrag.spv 

/usr/bin/glslc lineShader.vert -o lineVert.spv
/usr/bin/glslc lineShader.frag -o lineFrag.spv 

/usr/bin/glslc rayIntersect.comp -o compute.spv
