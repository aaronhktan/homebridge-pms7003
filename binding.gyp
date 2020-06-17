{
  "targets": [
    {
      "target_name": "homebridge-pms7003",
      "cflags!": [ "-fno-exceptions" ],
      "cflags_cc!": [ "-fno-exceptions" ],
      "sources": [
        "src/binding/binding.cpp",
        "src/binding/binding_utils.cpp",
        "src/c/pms7003.c"
      ],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")",
        "src/c",
        "src/binding"
      ],
      'defines': [ 'NAPI_DISABLE_CPP_EXCEPTIONS' ],
    }
  ]
}
