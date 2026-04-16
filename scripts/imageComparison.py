import argparse
import cv2
import numpy as np
from skimage.metrics import structural_similarity as ssim

def createHeatmap(ref: np.ndarray, img: np.ndarray, output_path: str) -> None:
    """Save a false-color error map where brighter/hotter colors mean larger pixel differences."""
    # Per-pixel absolute color difference aggregated to a single-channel heatmap.
    abs_diff = np.abs(ref - img)
    diff_map = np.mean(abs_diff, axis=2)
    heatmap_uint8 = np.uint8(np.clip(diff_map * 255.0, 0, 255))
    heatmap_color = cv2.applyColorMap(heatmap_uint8, cv2.COLORMAP_INFERNO)

    if not cv2.imwrite(output_path, heatmap_color):
        raise RuntimeError(f"Failed to save heatmap to '{output_path}'")

def computeMSE(ref: np.ndarray, img: np.ndarray) -> float:
    """Return Mean Squared Error (MSE): lower is better, 0 means identical images.

    For normalized images in [0, 1], rough guidance is:
    - excellent: < 1e-3
    - acceptable: 1e-3 to 1e-2
    - poor: > 1e-2
    """
    diff = ref - img
    return float(np.mean(diff * diff))

def computePSNR(ref: np.ndarray, img: np.ndarray, mseValue: float) -> float:
    """Return Peak Signal-to-Noise Ratio (PSNR) in dB: higher is better.

    Typical interpretation:
    - > 40 dB: excellent quality
    - 30 to 40 dB: good quality
    - 20 to 30 dB: visible degradation
    - < 20 dB: poor quality
    """
    if mseValue == 0.0:
        return float("inf")
    return float(10.0 * np.log10(1.0 / mseValue))

def computeSSIM(ref: np.ndarray, img: np.ndarray) -> float:
    """Return Structural Similarity Index (SSIM) in [-1, 1], usually [0, 1] for images.

    1.0 means identical structure. Practical guidance:
    - > 0.98: excellent
    - 0.95 to 0.98: good
    - 0.90 to 0.95: noticeable differences
    - < 0.90: often visibly degraded
    """
    return float(ssim(ref, img, channel_axis=-1, data_range=1.0))

def loadImages(refImage: str, renderImg: str) -> tuple[np.ndarray, np.ndarray]:
    """Load two color images, normalize to float32 in [0, 1], and validate shape match."""
    ref = cv2.imread(refImage, cv2.IMREAD_COLOR)
    img = cv2.imread(renderImg, cv2.IMREAD_COLOR)

    if ref is None:
        raise FileNotFoundError(f"Could not load reference image: '{refImage}'")
    if img is None:
        raise FileNotFoundError(f"Could not load reconstructed image: '{renderImg}'")
    
    # Convert to float [0,1]
    ref = ref.astype(np.float32) / 255.0
    img = img.astype(np.float32) / 255.0

    if ref.shape != img.shape:
        raise ValueError(
            f"Image size mismatch: ref={ref.shape}, img={img.shape}. "
            "Use images with identical resolution and channels."
        )

    return (ref,img)


def parseArguments() -> argparse.Namespace:
    """Parse command-line arguments for input image paths and optional heatmap output path."""
    parser = argparse.ArgumentParser(description="Compute MSE, PSNR, SSIM between two images")

    parser.add_argument("ref", help="Reference image")
    parser.add_argument("img", help="Reconstructed image")

    parser.add_argument("--save-heatmap", dest="save_heatmap", help="Path to save heatmap image")

    args = parser.parse_args()

    return args

def main():
    """Run the full comparison workflow and print metrics to stdout."""
    args = parseArguments()

    refImg, rendImg = loadImages(args.ref, args.img)

    mseVal = computeMSE(refImg, rendImg)
    psnrVal = computePSNR(refImg, rendImg, mseVal)
    ssimVal = computeSSIM(refImg, rendImg)

    print(f"MSE:  {mseVal:.8f}")
    if np.isinf(psnrVal):
        print("PSNR: inf")
    else:
        print(f"PSNR: {psnrVal:.4f} dB")
    print(f"SSIM: {ssimVal:.6f}")

    if args.save_heatmap:
        createHeatmap(refImg, rendImg, args.save_heatmap)
        print(f"Heatmap saved to: {args.save_heatmap}")

if __name__ == "__main__":
    main()