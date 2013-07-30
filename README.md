##node-pngquant-native

node-pngquant-native是一个node的native插件，而不是调用命令行的方式，提升处理速率，有效的省去了调用外部程序产生的时间开销。

##安装

    npm install -g node-pngquant-native

##使用

```javascript

var C = require('node-pngquant-native');

fs.readFile('./alphatest.png', function (err, data) {
  if (err) throw err;
  var buffer = C.option({
    params: '-v --iebug'
  }).compress(data);
  fs.writeFile('./alphatest_out.png', buffer, {
      flags: 'wb'
  }, function(err){});
});


```
