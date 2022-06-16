// bd 2014.09.25
// big ups to Jim Bumgardner
// much of this code was inspired from his blog here:
// http://krazydad.com/tutorials/makecolors.php
// I wrote this to output a sine wave table in hex so I could
// program a color cycling LED on an ATtiny10.

/*
render this on the following html page:

<html>
<body>
<div id="colors"></div>
<pre id="log"></pre>
</body>
</html>

*/

var content = '', log = '';

function SineTable( period, frequency ) {
    var i;
    var periodRadians = 2 * Math.PI * frequency;
    var halfPeriod = ( period - 1 ) / 2;
    var table = [];

    for (i = 0; i < period; ++i) {
        var iPercentOfPeriod = i / period;
        var radians = iPercentOfPeriod * periodRadians;
        var s = Math.sin(frequency * radians);
        
        sd = s * halfPeriod + halfPeriod; // denormalize over 0..period-1

        if ( sd == period ) {
             log += 'sd is period, s is ' + s + ', half period is ' + halfPeriod + '\n';   
        }
        
        log += i + ' (' + (iPercentOfPeriod * 100).toFixed(2) + '%): sin(θ:' + radians.toFixed(2) + ') == ' + s.toFixed(2) + ', denormalized == ' + sd.toFixed(2) + '\n';
        
        table.push(sd);
    }

    return table;
}

function Hexify(n) {
    var hexTable = "0123456789abcdef";

    return String(hexTable.substr(n >> 4 & 0x0f, 1)) + hexTable.substr(n & 0x0f, 1);
}

function Block(r, g, b, p) {
    if (typeof (p) === 'undefined') p = 255;
    
    var css = 'r-' + r + '-g-' + g + '-b-' + b;

    r = Hexify(r / p * 255);
    g = Hexify(g / p * 255);
    b = Hexify(b / p * 255);
    
    return '<font class="' + css + '" color="#' + r + g + b + '">&#9608;</font>';
}

function Gradient( table, gOffset, bOffset ) {
    if ( typeof(gOffset) === 'undefined' ) gOffset = 0;
    if ( typeof(bOffset) === 'undefined' ) bOffset = 0;
    
    var b, g, i, n, r, s;
    
    for ( i = 0, n = table.length, s = ''; i < n; ++i ) {
        r = table[ i ];
        g = table[ ( i + gOffset ).toFixed(0) % n ];
        b = table[ ( i + bOffset ).toFixed(0) % n ];
        
        s += Block( r, g, b, n - 1 );
    }
    
    return '<div>' + s + '</div>';
}

var f = 1;
var p = 360;
// report parameters for basic sine wave over each integral degree with two offset waves
var t = SineTable();
SineTable(p, f, 2 * Math.PI / 3, t );
SineTable(p, f, 4 * Math.PI / 3, t );

log += '--------------------------------------------------\n';

SineTable( 256, 1 )
SineTable( 256, 2 )

$('#log').html( log );

var tests = [
    [ 360, 1 ],
    [ 256, 1 ],
    [ 256, 2 ],
    [ 128, 2 ],
];

for ( i = 0, n = tests.length; i < n; ++i ) {
    var frequency = tests[ i ][ 1 ];
    var period = tests[ i ][ 0 ];
    var t = SineTable( period, frequency );

    content += Gradient( t );
    content += Gradient( t, period / 3, 2 * period / 3 );

    $('#log').append( 'p: ' + period + ', f: ' + frequency + '\n' );
    for ( var j = 0; j < period; ) {
        for ( var k = 0; k < 32; ++j, ++k ) {
            $('#log').append( '0x' + Hexify( t[ j ] ) + ',' );
        }
        $('#log').append( '    \\\n' );
    }
}

$('#colors').html(content);
