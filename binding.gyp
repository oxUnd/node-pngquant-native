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
            'src/libpng/png.c',
            'src/libpng/pngerror.c',
            'src/libpng/pngget.c',
            'src/libpng/pngmem.c',
            'src/libpng/pngpread.c',
            'src/libpng/pngread.c',
            'src/libpng/pngrio.c',
            'src/libpng/pngrtran.c',
            'src/libpng/pngrutil.c',
            'src/libpng/pngset.c',
            'src/libpng/pngtrans.c',
            'src/libpng/pngwio.c',
            'src/libpng/pngwrite.c',
            'src/libpng/pngwtran.c',
            'src/libpng/pngwutil.c',
        ],  
        "include_dirs": [
            "<!(node -e \"require('nan')\")",
	    "./src/libpng"
        ],
        'conditions': [
            ['OS == "win"', {
            }, {
                'libraries': [
                    '-lz',
                    '-lm'
                ]
            }]
        ],
    }]
}
