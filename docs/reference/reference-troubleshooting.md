# Troubleshooting

## Common issues and solutions

| Issue                   | Possible Cause                          | Solution                                                                                  |
| ----------------------- | --------------------------------------- | ----------------------------------------------------------------------------------------- |
| Slow inference          | CPU-only model on large parameter count | Use GPU-optimized model variants when available                                           |
| Model download failures | Network connectivity issues             | Check your internet connection, try `foundry cache list` to verify cache state            |
| Service won't start     | Port conflicts or permission issues     | Try `foundry service restart` or post an issue providing logs with `foundry zip-logsrock` |

## Diagnosing performance issues

If you're experiencing slow inference:

1. Check that you're using GPU acceleration if available
2. Monitor memory usage during inference to detect bottlenecks
3. Consider a more quantized model variant (e.g., INT8 instead of FP16)
4. Experiment with batch sizes for non-interactive workloads
