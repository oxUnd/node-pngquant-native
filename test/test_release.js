var C = require('node-pngcrush');
var fs = require('fs');


fs.readFile('./alphatest.png', function (err, data) {
  if (err) throw err;
  var buffer = C.compress(data);
  fs.writeFile('./alphatest_out.png', buffer, {
      flags: 'wb'
  }, function(err){});
});

