{
  "targets": [
    {
      "target_name": "foundry_local_napi",
      "sources": ["foundry_local_napi.c"],
      "include_dirs": [
        "<!@(node -p \"require('node-api-headers').include_dir\")"
      ],
      "defines": ["NAPI_VERSION=4"],
      "conditions": [
        ["OS=='win'", {
          "msvs_settings": {
            "VCCLCompilerTool": {
              "WarningLevel": 3
            }
          }
        }],
        ["OS=='mac'", {
          "xcode_settings": {
            "CLANG_CXX_LANGUAGE_STANDARD": "c11",
            "MACOSX_DEPLOYMENT_TARGET": "11.0"
          }
        }],
        ["OS=='linux'", {
          "cflags": ["-std=c11", "-Wall", "-Wextra"]
        }]
      ]
    }
  ]
}
