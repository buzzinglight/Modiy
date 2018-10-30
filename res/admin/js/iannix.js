/*
    This file is part of IanniX, a graphical real-time open-source sequencer for digital art
    Copyright (C) 2010-2014 — IanniX Association

    Project Manager: Thierry Coduys (http://www.le-hub.org)
    Development:     Guillaume Jacquemin (http://www.buzzinglight.com)

    This file was written by Guillaume Jacquemin.

    IanniX is a free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


//Prototypes for Strings
String.prototype.trim = function()
	{ return (this.replace(/^[\s\xA0]+/, "").replace(/[\s\xA0]+$/, "")) }
String.prototype.startsWith = function(str) 
	{ return (this.match("^"+str)==str) }
String.prototype.endsWith = function(str) 
	{ return (this.match(str+"$")==str) }
String.prototype.replaceAll = function(str, str2)
	{ return (this.replace(new RegExp(str, 'g'), str2)) }
String.prototype.pad = function(length) {
	var str = '' + this;
    while (str.length < length) {
        str = '0' + str;
    }
    return str;
}
Array.prototype.clean = function(deleteValue) {
  for (var i = 0; i < this.length; i++) {
    if (this[i] == deleteValue) {         
      this.splice(i, 1);
      i--;
    }
  }
  return this;
};


//Constants
var E          = Math.E;
var LN2        = Math.LN2;
var LN10       = Math.LN10;
var LOG2E      = Math.LOG2E;
var LOG10E     = Math.LOG10E;
var PI         = Math.PI;
var TWO_PI	   = 2 * Math.PI;
var THIRD_PI   = Math.PI / 3;
var QUARTER_PI = Math.PI / 4;
var HALF_PI    = Math.PI / 2;
var SQRT1_2    = Math.SQRT1_2;
var SQRT2      = Math.SQRT2;

//Math functions
function abs(x) 		{	return Math.abs(x);		}
function acos(x) 		{	return Math.acos(x);	}
function asin(x) 		{	return Math.asin(x);	}
function atan(x) 		{	return Math.atan(x);	}
function atan2(x,y) 	{	return Math.atan2(x,y);	}
function ceil(x) 		{	return Math.ceil(x);	}
function cos(x) 		{	return Math.cos(x);		}
function exp(x) 		{	return Math.exp(x);		}
function floor(x) 		{	return Math.floor(x);	}
function log(x) 		{	return Math.log(x);		}
function min(x,y) 		{	return Math.min(x,y);	}
function max(x,y) 		{	return Math.max(x,y);	}
function pow(x,y) 		{	return Math.pow(x,y);	}
function sin(x) 		{	return Math.sin(x);		}
function sqrt(x) 		{	return Math.sqrt(x);	}
function sq(x)			{	return x*x;				}
function tan(x) 		{	return Math.tan(x);		}
function degrees(value) {	return value * 180. / pi;  }
function radians(value) {	return value * pi / 180.;  }
function round(x, y) 	{
	if(y == undefined)	return Math.round(x);
	else 				return Math.round(x*Math.pow(10, y)) / Math.pow(10, y);
}
function roundAndPad(x, y) {
	var s = round(x, y).toString();
	if (s.indexOf('.') == -1) s += '.';
	while (s.length < s.indexOf('.') + (y+1)) s += '0';
	return s;
}
function random(low, high) {
	if((low == undefined) || (high == undefined))
		return Math.random();
	else
		return range(Math.random(), low, high);
}

//Useful functions
function constrain(value, min, max) {
	return Math.min(max, Math.max(min, value));
}
function dist(x1, y1, z1, x2, y2, z2) {
	var dx = x2 - x1, dy = y2 - y1, dz = z2 - z1;
	return Math.sqrt(sq(dx) + sq(dy) + sq(dz));
}
function angle(x1, y1, x2, y2) {
	var dx = x2 - x1, dy = y2 - y1, angle = 0;
	if((dx > 0) && (dy >= 0))
	    angle = (Math.atan(dy / dx)) * 180.0 / PI;
	else if((dx <= 0) && (dy > 0))
	    angle = (-Math.atan(dx / dy) + HALF_PI) * 180.0 / PI;
	else if((dx < 0) && (dy <= 0))
	    angle = (Math.atan(dy / dx) + PI) * 180.0 / PI;
	else if((dx >= 0) && (dy < 0))
	    angle = (-Math.atan(dx / dy) + 3 * HALF_PI) * 180.0 / PI;
	return angle;
}
function norm(value, low, high) {
	if((high - low) == 0)
		return low;
	else
		return (value - low) / (high - low);
}
function range(value, low, high) {
	return value * (high - low) + low; 
}
function rangeMid(value, low, mid, high) {
	if(value < 0.5)
		return (value * 2) * (mid - low) + low;
	else
		return (value - .5) * 2 * (high - mid) + mid;
}
function map(value, low1, high1, low2, high2, power) {
	if(power == undefined)
		return range(norm(value, low1, high1), low2, high2);
	else
		return ((value-low1)/(high1-low1) == 0) ? low2 : (((value-low1)/(high1-low1)) > 0) ? (low2 + (high2-low2) * pow((value-low1)/(high1-low1), power)) : ( low2 + (high2-low2) * -(pow(((-value+low1)/(high1-low1)), power)));
}