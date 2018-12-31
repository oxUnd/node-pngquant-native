{
    'targets': [{
        'target_name': 'binding',
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
            'src/binding.cc',
            'src/PQCompress.cc',
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
            "./gyp/gyp/libpng.gyp:libpng"
        ],
        "include_dirs": [
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
