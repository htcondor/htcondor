/*
 * Copyright 2013 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
'use strict';

var HeatGrid = function(el) {
  this.el = $(el);

  var frac = function(minval, maxval, fraction) {
    return minval + fraction * (maxval - minval);
  };

  var gradient = function(mincolor, zerocolor, maxcolor,
                          ofs) {
    if (ofs == 0) {
      return zerocolor;
    } else if (ofs < 0) {
      return [frac(zerocolor[0], mincolor[0], -ofs / 2),
              frac(zerocolor[1], mincolor[1], -ofs / 2),
              frac(zerocolor[2], mincolor[2], -ofs / 2)];
    } else if (ofs > 0) {
      return [frac(zerocolor[0], maxcolor[0], ofs / 2),
              frac(zerocolor[1], maxcolor[1], ofs / 2),
              frac(zerocolor[2], maxcolor[2], ofs / 2)];
    }
  };

  this.draw = function(grid) {
    console.debug('heatgrid.draw', grid);
    this.el.html('<div id="heatgrid" tabindex=0><canvas></canvas>' +
                 '<div id="heatgrid-popover"></div>' +
                 '<div id="heatgrid-highlight"></div></div>');
    var heatgrid = this.el.find('#heatgrid');
    heatgrid.css({
      position: 'relative',
      overflow: 'scroll',
      width: '100%',
      height: '100%'
    });
    var popover = this.el.find('#heatgrid-popover');
    popover.css({
      position: 'absolute',
      background: '#aaa',
      border: '1px dotted black',
      'white-space': 'pre'
    });
    var highlight = this.el.find('#heatgrid-highlight');
    highlight.css({
      position: 'absolute',
          //background: 'rgba(255,192,192,16)',
      width: '3px',
      height: '3px',
      margin: '0',
      padding: '0',
      border: '1px solid red'
    });
    var canvas = this.el.find('canvas');
    var xmult = parseInt(1000 / grid.headers.length);
    if (xmult < 1) xmult = 1;
    var xsize = grid.headers.length * xmult;
    var ysize = grid.data.length;
    var vysize = ysize < 400 ? 400 : ysize;
    canvas.attr({width: xsize, height: ysize});
    canvas.css({
      background: '#fff',
      width: '100%',
      height: vysize
    });
    console.debug('heatgrid canvas size is: x y =', xsize, ysize);
    var ctx = canvas[0].getContext('2d');

    if (!grid.data.length || !grid.data[0].length) {
      return;
    }

    var lastPos = { x: 0, y: 0 };
    var movefunc = function(x, y) {
      if (x >= grid.headers.length) x = grid.headers.length - 1;
      if (y >= grid.data.length) y = grid.data.length - 1;
      if (x < 0) x = 0;
      if (y < 0) y = 0;
      lastPos.x = x;
      lastPos.y = y;
      var info = [];
      for (var i = 0; i < grid.headers.length; i++) {
        if (grid.types[i] != 'number') {
          info.push(grid.data[y][i]);
        } else {
          break;
        }
      }
      info.push(grid.headers[x]);
      info.push('value=' + grid.data[y][x]);

      var psize_x = canvas.width() / grid.headers.length;
      var psize_y = canvas.height() / grid.data.length;
      var cx = x * psize_x;
      var cy = y * psize_y;
      highlight.css({left: cx, top: cy});
      if (cx < canvas.width() / 2) {
        popover.css({
          left: cx + 30,
          right: 'auto'
        });
      } else {
        popover.css({
          left: 'auto',
          right: heatgrid[0].clientWidth - cx + 10
        });
      }
      if (cy < canvas.height() / 2) {
        popover.css({
          top: cy + 10,
          bottom: 'auto'
        });
      } else {
        popover.css({
          top: 'auto',
          bottom: heatgrid[0].clientHeight - cy + 10
        });
      }
      popover.text(info.join('\n'));
    };
    heatgrid.mousemove(function(ev) {
      var pos = canvas.position();
      var offX = ev.pageX - pos.left - 1;
      var offY = ev.pageY - pos.top - 1;
      var x = parseInt(offX / canvas.width() * grid.headers.length);
      var y = parseInt(offY / canvas.height() * grid.data.length);
      movefunc(x, y);
    });
    heatgrid.keydown(function(ev) {
      if (ev.which == 38) { // up
        movefunc(lastPos.x, lastPos.y - 1);
      } else if (ev.which == 40) { // down
        movefunc(lastPos.x, lastPos.y + 1);
      } else if (ev.which == 37) { // left
        movefunc(lastPos.x - 1, lastPos.y);
      } else if (ev.which == 39) { // right
        movefunc(lastPos.x + 1, lastPos.y);
      } else {
        return true; // propagate event forward
      }
      ev.stopPropagation();
      return false;
    });
    heatgrid.mouseleave(function() {
      popover.hide();
    });
    heatgrid.mouseenter(function() {
      heatgrid.focus(); // so that keyboard bindings work
      popover.show();
    });

    var total = 0, count = 0;
    for (var y = 0; y < grid.data.length; y++) {
      for (var x = 0; x < grid.data[y].length; x++) {
        if (grid.types[x] != 'number') continue;
        var cell = parseFloat(grid.data[y][x]);
        if (!isNaN(cell)) {
          total += cell;
          count++;
        }
      }
    }
    var avg = total / count;

    var tdiff = 0;
    for (var y = 0; y < grid.data.length; y++) {
      for (var x = 0; x < grid.data[y].length; x++) {
        if (grid.types[x] != 'number') continue;
        var cell = parseFloat(grid.data[y][x]);
        if (!isNaN(cell)) {
          tdiff += (cell - avg) * (cell - avg);
        }
      }
    }
    var stddev = Math.sqrt(tdiff / count);
    console.debug('total,count,mean,stddev = ', total, count, avg, stddev);

    var img = ctx.createImageData(xsize, ysize);
    for (var y = 0; y < grid.data.length; y++) {
      for (var x = 0; x < grid.data[y].length; x++) {
        if (grid.types[x] != 'number') continue;
        var cell = parseFloat(grid.data[y][x]);
        if (isNaN(cell)) continue;
        var ofs = stddev ? (cell - avg) / stddev : 3;
        var color = gradient([192, 192, 192],
                             [192, 192, 255],
                             [0, 0, 255], ofs);
        if (!color) continue;
        var pix = (y * xsize + x * xmult) * 4;
        for (var i = 0; i < xmult; i++) {
          img.data[pix + 0] = color[0];
          img.data[pix + 1] = color[1];
          img.data[pix + 2] = color[2];
          img.data[pix + 3] = 255;
          pix += 4;
        }
      }
    }
    ctx.putImageData(img, 0, 0);
  };
};
