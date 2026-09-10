# Third-party notices

This Java POC is MIT licensed; see the repository LICENSE, also included in the JAR.

The Java dependency is Java Native Access (JNA) 5.17.0, used under its
Apache License 2.0 option. JNA is distributed as an unmodified, separate JAR;
retain its embedded licenses and notices (including its native libffi notices).
Source: https://github.com/java-native-access/jna/tree/5.17.0

Native runtime preparation is separate and explicit. Microsoft.AI.Foundry.Local.Runtime
2.0.1, Microsoft.ML.OnnxRuntime 1.28.0 and Microsoft.ML.OnnxRuntimeGenAI.Foundry
0.15.2 have MIT top-level licenses and additional third-party notices. The preparation
script retains the packages' license/notice files. Native packages and model weights
are not included in the Java JAR or this source tree.

Model licenses are independent of the SDK license. Inspect exact catalog model
metadata and its linked license before an explicit download. The CLI requires a
separate `--accept-model-license` flag; the Java API leaves informed consent to
its caller. No model license is accepted by class initialization or inference.

The downloaded Nemotron generic CPU version 3 has a Microsoft MIT `LICENSE`,
but `NOTICES` additionally references the NVIDIA Open Model License and Silero
VAD's MIT license. The current upstream NVIDIA model card instead identifies
OpenMDW-1.1. The catalog's separate license-description URL returned HTTP 404
during local evaluation. Retain these distinctions; the SDK's license does not
resolve third-party model terms or grant redistribution rights. No model or
native bundle is published by this project.
