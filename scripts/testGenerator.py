import json
import math

def create_test(alg, sampleCount, width, height, fov):
    return {
        "alg": alg,
        "sampleCount": sampleCount,
        "width": width,
        "height": height,
        "fov": fov
    }

tests = []

# Example parameter sets
pixelResolutions = [(800, 600), (800, 800), (800, 1000), (800, 1200), (800, 1400), (800, 1600), (800, 1800), (800, 2000),
                    (1000, 1760), (1200, 1600), (1600, 1300), (2000,1120), (2000, 1280), (2000, 1360), (2000, 1440), 
                    (2000, 1520), (2000, 1600), (2000, 1680), (2000, 1760), (2000, 1840), (2000, 1920), (2000, 2000),
                    (2080, 2000), (2160, 2000), (2240, 2000), (2320, 2000), (2400, 2000), (2480, 2000), (2560, 2000),
                    (2640, 2000), (2720, 2000), (2800, 2000), (2880, 2000), (2960, 2000), (3040, 2000), (3120, 2000),
                    (3200, 2000), (3280, 2000), (3360, 2000), (3440, 2000), (3520, 2000), (3600, 2000)]
standardResolutions = [(640, 360), (1280, 720), (1920, 1080), (2560, 1440), (3840, 2160)]
fovs = [60, 45]
resolution = [1280, 720]

# Generate tests for NovelColor
# for res in pixelResolutions:
#     for alg in ["NovelColor", "NovelDepth", "NovelAngle"]:
#             tests.append(create_test(alg, 128, res[0], res[1], math.radians(fovs[0])))
#     for alg in ["NovelAnalytic", "PointCloud"]:
#             tests.append(create_test(alg, None, res[0], res[1], math.radians(fovs[1])))

# for res in standardResolutions:
#     for alg in ["NovelColor", "NovelDepth", "NovelAngle"]:
#             tests.append(create_test(alg, 128, res[0], res[1], math.radians(fovs[0])))
#     for alg in ["NovelAnalytic", "PointCloud"]:
#             tests.append(create_test(alg, None, res[0], res[1], math.radians(fovs[1])))

# for sc in range(16, 513, 16):
#     for alg in ["NovelColor", "NovelDepth", "NovelAngle"]:
#         tests.append(create_test(alg, sc, resolution[0], resolution[1], math.radians(fovs[0])))

for alg in ["NovelColor", "NovelDepth", "NovelAngle"]:      # do for different cam counts but it has to be different setup for each
        tests.append(create_test(alg, 128, resolution[0], resolution[1], math.radians(fovs[0])))

# Algorithms without sampleCount
tests.append(create_test("NovelAnalytic", None, resolution[0], resolution[1], math.radians(fovs[1])))
tests.append(create_test("PointCloud", None, resolution[0], resolution[1], math.radians(fovs[1])))

# Final JSON
data = {
    "setup": "baked_8camGrid_random",
    "precision": True,
    "tests": tests
}

# Write to file
with open("sample8CamsBaked.json", "w") as f:
    json.dump(data, f, indent=2)

print(f"Generated {len(tests)} tests.")