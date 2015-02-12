{
    'targets': [{
        'target_name': 'pngquant_native',
        'cflags': [
            '-DNO_ALONE',
            '-DDEBUG',
            '-fno-inline',
            '-O0',
            '-fstrict-aliasing', 
            '-ffast-math',
            '-funroll-loops', 
            '-fomit-frame-pointer', 
            '-ffinite-math-only',
            '-std=c99'
        ],
        'sources': [
            'src/pngquant_native.cpp',
            'src/pngquant/pngquant.c',
            'src/pngquant/rwpng.c',
            'src/pngquant/lib/pam.c',
            'src/pngquant/lib/mediancut.c',
            'src/pngquant/lib/blur.c',
            'src/pngquant/lib/mempool.c',
            'src/pngquant/lib/viter.c',
            'src/pngquant/lib/nearest.c', 
            'src/pngquant/lib/libimagequant.c',
        ],  
        "dependencies": [
            "./gyp/gyp/zlib.gyp:zlib"
        ],
        "include_dirs": [
            "<!(node -e \"require('nan')\")",
        ],
        'conditions': [
            ['OS == "win"', {
            }, {
                'libraries': [
                    '-lm'
                ]
            }]
        ],
    }]
}
