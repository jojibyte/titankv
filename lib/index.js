const bindings = require('bindings');
const native = bindings('titankv');

module.exports = {
    TitanKV: native.TitanKV
};
