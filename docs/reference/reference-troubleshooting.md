# Troubleshooting

## Common issues and solutions

| Issue                   | Possible Cause                          | Solution                                                                                  |
| ----------------------- | --------------------------------------- | ----------------------------------------------------------------------------------------- |
| Slow inference          | CPU-only model on large parameter count | Use GPU-optimized model variants when available                                           |
| Model download failures | Network connectivity issues             | Check your internet connection, try `foundry cache list` to verify cache state            |
| Service won't start     | Port conflicts or permission issues     | Try `foundry service restart` or post an issue providing logs with `foundry zip-logsrock` |
| Qualcomm NPU error (`Qnn error code 5005: "Failed to load from EpContext model. qnn_backend_manager."`) | Qualcomm NPU error | Under investigation |
| `winget install Microsoft.FoundryLocal --scope machine` fails with “The current system configuration does not support the installation of this package.” | Winget blocks MSIX machine-scope installs due to an OS bug when using provisioning APIs from a packaged context | Use `Add-AppxProvisionedPackage` instead. Download the `.msix` and its dependency, then run in **elevated** PowerShell: `Add-AppxProvisionedPackage -Online -PackagePath .\FoundryLocal.msix -DependencyPackagePath .\VcLibs.appx -SkipLicense`. This installs Foundry Local for all users.|
| QNN graph execute error (Error 6031)    | NPU model issue    | Under investigation. Try using a different model or the equivalent cpu model in the meantime. |
## Diagnosing performance issues

If you're experiencing slow inference:

1. Check that you're using GPU acceleration if available
2. Monitor memory usage during inference to detect bottlenecks
3. Consider a more quantized model variant (e.g., INT8 instead of FP16)
4. Experiment with batch sizes for non-interactive workloads
