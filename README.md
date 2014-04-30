## node-pngquant-native
[![NPM version](https://badge.fury.io/js/node-pngquant-native.png)](http://badge.fury.io/js/node-pngquant-native)

node-pngquant-native是一个node的native插件，而不是调用命令行的方式，提升处理速率，有效的省去了调用外部程序产生的时间开销。

## install

    npm install -g node-pngquant-native
    
### require
+ 编译pngquant，编译器必须支持C99
+ 符合[node-gyp环境要求](https://github.com/TooTallNate/node-gyp#installation)
+ Windows用户，如果安装失败，请clone win32分支进行编译

## use

```javascript

var pngquant = require('node-pngquant-native');

fs.readFile('./alphatest.png', function (err, buffer) {
  if (err) throw err;
  var resBuffer = pngquant.compress(buffer, {
    "speed": 1 //1 ~ 11
  });

  fs.writeFile('./alphatest_out.png', resBuffer, {
      flags: 'wb'
  }, function(err){});
});

```

## Api

### pngquant.`compress(buffer, option)`

```javascript
var pngquant = require('node-pngquant-native')
var option = {
    speed: 11
    //...    
}

var resBuffer = pngquant.compress(buffer, option);

```
#### option

+ option.`speed` 

    Speed/quality trade-off from 1 (brute-force) to 11 (fastest). The default is 3. Speed 10 has 5% lower quality, but is 8 times faster than the default. Speed 11 disables dithering and lowers compression level.

    ```javascript
    var opt = {
        speed: 11
    }
    ```

+ option.`quality = [min, max]`

    min and max are numbers in range 0 (worst) to 100 (perfect), similar to JPEG. pngquant will use the least amount of colors required to meet or exceed the max quality. If conversion results in quality below the min quality the image won't be saved (if outputting to stdin, 24-bit original will be output) and pngquant will exit with status code 99.

    ```javascript
    var opt = {
        quality: [40, 60]
    }
    ```

+ option.`iebug`
    
    Workaround for IE6, which only displays fully opaque pixels. pngquant will make almost-opaque pixels fully opaque and will avoid creating new transparent colors.

    ```javascript
    var opt = {
        isbug: true
    }
    ```