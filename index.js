var _handle = require('./binding.js');

function Pngquant(buffer) {
    this._option = '';
    this._callback = function() {};
    this._handle = new _handle.Pngquant();
}

Pngquant.prototype = {
    option: function (opt) {
        opt = opt || {};
        opt.filename = opt.filename || 'stdin.png';
        if (opt.params) {
            opt = opt.params + ' ' + opt.filename;
        } else {
            opt = opt.filename;
        }
        this._option = opt;
        return this;
    },
    compress: function(buffer, cb) {
        if (cb) this._callback = cb;
        out = this._handle.compress(buffer, this._option, this._callback);
        return out;
    }
};

module.exports = new Pngquant;
