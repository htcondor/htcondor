/*
 * Copyright 2012 Google Inc. All Rights Reserved.
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
/*
 * 2015 Changes by Todd Tannenbaum and Alan De Smet, 
 *      Center for High Throughput Computing,
 *      University of Wisconsin - Madison,
 *      as part of integration with HTCondor.
 */
'use strict';

var afterquery = (function() {
  // To appease v8shell
  var console, localStorage;
  try {
    console = window.console;
  }
  catch (ReferenceError) {
    console = {
      debug: print
    };
  }
  try {
    localStorage = window.localStorage;
  } catch (ReferenceError) {
    localStorage = {};
  }

  // For konqueror compatibility
  if (!console) {
    console = window.console;
  }
  if (!console) {
    console = {
      debug: function() {}
    };
  }


  function err(s) {
    $('#vizlog').append('\n' + s);
  }


  function showstatus(s, s2) {
    $('#statustext').html(s);
    $('#statussub').text(s2 || '');
    if (s || s2) {
      console.debug('status message:', s, s2);
      $('#vizstatus').show();
    } else {
      $('#vizstatus').hide();
    }
  }


  function parseArgs(query) {
    var kvlist;
    if (query.join) {
      // user provided an array of 'key=value' strings
      kvlist = query;
    } else {
      // assume user provided a single string
      if (query[0] == '?' || query[0] == '#') {
        query = query.substr(1);
      }
      kvlist = query.split('&');
    }
    var out = {};
    var outlist = [];
    for (var i in kvlist) {
      var kv = kvlist[i].split('=');
      var key = decodeURIComponent(kv.shift());
      var value = decodeURIComponent(kv.join('='));
      out[key] = value;
      outlist.push([key, value]);
    }
    console.debug('query args:', out);
    console.debug('query arglist:', outlist);
    return {
      get: function(key) { return out[key]; },
      all: outlist
    };
  }


  var IS_URL_RE = RegExp('^(http|https)://');

  function looksLikeUrl(s) {
    var url, label;
    var pos = (s || '').lastIndexOf('|');
    if (pos >= 0) {
      url = s.substr(0, pos);
      label = s.substr(pos + 1);
    } else {
      url = s;
      label = s;
    }
    if (IS_URL_RE.exec(s)) {
      return [url, label];
    } else {
      return;
    }
  }


  function htmlEscape(s) {
    if (s == undefined) {
      return s;
    }
    return s.replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;')
        .replace(/\n/g, '<br>\n');
  }


  function dataToGvizTable(grid, options) {
    if (!options) options = {};
    var is_html = options.allowHtml;
    var headers = grid.headers, data = grid.data, types = grid.types;
    var dheaders = [];
    for (var i in headers) {
      dheaders.push({
        id: headers[i],
        label: headers[i],
        type: (types[i] != T_BOOL || !options.bool_to_num) ? types[i] : T_NUM
      });
    }
    var ddata = [];
    for (var rowi in data) {
      var row = [];
      for (var coli in data[rowi]) {
        var cell = data[rowi][coli];
        if (is_html && types[coli] === T_STRING) {
          cell = cell.toString();
          var urlresult = looksLikeUrl(cell);
          if (urlresult) {
            cell = '<a href="' + encodeURI(urlresult[0]) + '">' +
                htmlEscape(urlresult[1]) + '</a>';
          } else {
            cell = htmlEscape(cell);
          }
        }
        var col = { v: cell };
        if (options.show_only_lastseg && col.v && col.v.split) {
          var lastseg = col.v.split('|').pop();
          if (lastseg != col.v) {
            col.f = lastseg;
          }
        }
        row.push(col);
      }
      ddata.push({c: row});
    }
    var datatable = new google.visualization.DataTable({
      cols: dheaders,
      rows: ddata
    });
    if (options.intensify) {
      var minval, maxval;
      var rowmin = [], rowmax = [];
      var colmin = [], colmax = [];
      for (var coli in grid.types) {
        if (grid.types[coli] !== T_NUM) continue;
        for (var rowi in grid.data) {
          var cell = grid.data[rowi][coli];
          if (cell < (minval || 0)) minval = cell;
          if (cell > (maxval || 0)) maxval = cell;
          if (cell < (rowmin[rowi] || 0)) rowmin[rowi] = cell;
          if (cell > (rowmax[rowi] || 0)) rowmax[rowi] = cell;
          if (cell < (colmin[coli] || 0)) colmin[coli] = cell;
          if (cell > (colmax[coli] || 0)) colmax[coli] = cell;
        }
      }

      for (var coli in grid.types) {
        if (grid.types[coli] == T_NUM) {
          var formatter = new google.visualization.ColorFormat();
          var mn, mx;
          if (options.intensify == 'xy') {
            mn = minval;
            mx = maxval;
          } else if (options.intensify == 'y') {
            console.debug(colmin, colmax);
            mn = colmin[coli];
            mx = colmax[coli];
          } else if (options.intensify == 'x') {
            throw new Error('sorry, intensify=x not supported yet');
          } else {
            throw new Error("unknown intensify= mode '" +
                            options.intensify + "'");
          }
          console.debug('coli=' + coli + ' mn=' + mn + ' mx=' + mx);
          formatter.addGradientRange(mn - 1, 0, null, '#f88', '#fff');
          formatter.addGradientRange(0, mx + 1, null, '#fff', '#88f');
          formatter.format(datatable, parseInt(coli));
        }
      }
    }
    return datatable;
  }


  var CANT_NUM = 1;
  var CANT_BOOL = 2;
  var CANT_DATE = 4;
  var CANT_DATETIME = 8;

  var T_NUM = 'number';
  var T_DATE = 'date';
  var T_DATETIME = 'datetime';
  var T_BOOL = 'boolean';
  var T_STRING = 'string';


  function guessTypes(data) {
    var impossible = [];
    for (var rowi in data) {
      var row = data[rowi];
      for (var coli in row) {
        impossible[coli] |= 0;
        var cell = row[coli];
        if (cell == '' || cell == null) continue;
        var d = myParseDate(cell);
        if (isNaN(d)) {
          impossible[coli] |= CANT_DATE | CANT_DATETIME;
        } else if (d.getHours() || d.getMinutes() || d.getSeconds()) {
          impossible[coli] |= CANT_DATE; // has time, so isn't a pure date
        }
        var f = cell * 1;
        if (isNaN(f)) impossible[coli] |= CANT_NUM;
        if (!(cell == 0 || cell == 1 ||
              cell == 'true' || cell == 'false' ||
              cell == true || cell == false ||
              cell == 'True' || cell == 'False')) impossible[coli] |= CANT_BOOL;
      }
    }
    console.debug('guessTypes impossibility list:', impossible);
    var types = [];
    for (var coli in impossible) {
      var imp = impossible[coli];
      if (!(imp & CANT_BOOL)) {
        types[coli] = T_BOOL;
      } else if (!(imp & CANT_DATE)) {
        types[coli] = T_DATE;
      } else if (!(imp & CANT_DATETIME)) {
        types[coli] = T_DATETIME;
      } else if (!(imp & CANT_NUM)) {
        types[coli] = T_NUM;
      } else {
        types[coli] = T_STRING;
      }
    }
    return types;
  }


  // We want to support various different date formats that people
  // tend to use as strings, with and without time of day,
  // including yyyy-mm-dd hh:mm:ss.mmm, yyyy/mm/dd, mm/dd/yyyy hh:mm PST, etc.
  // This gets a little hairy because so many things are optional.
  // We could try to support two-digit years or dd-mm-yyyy formats, but
  // those create parser ambiguities, so let's avoid it.
  var DATE_RE1 = RegExp(
      '^(\\d{1,4})[-/](\\d{1,2})(?:[-/](\\d{1,4})' +
      '(?:[T\\s](\\d{1,2}):(\\d\\d)(?::(\\d\\d)(?:\\.(\\d+))?)?)?)?' +
      '(?: \\w\\w\\w)?$');
  // Some people (gviz, for example) provide "json" files where dates
  // look like javascript Date() object declarations, eg.
  // Date(2014,0,1,2,3,4)
  var DATE_RE2 = /^Date\(([\d,]+)\)$/;
  function myParseDate(s) {
    if (s == null) return s;
    if (s && s.getDate) return s;

    // Try gviz-style Date(...) format first
    var g = DATE_RE2.exec(s);
    if (g) {
      g = (',' + g[1]).split(',');
      if (g.length >= 3) {
        g[2]++;  // date objects start at month=0, for some reason
      }
    }

    // If that didn't match, try string-style date format
    if (!g || g.length > 8) g = DATE_RE1.exec(s);
    if (g && (g[3] > 1000 || g[1] > 1000)) {
      if (g[3] > 1000) {
        // mm-dd-yyyy
        var yyyy = g[3], mm = g[1], dd = g[2];
      } else {
        // yyyy-mm-dd
        var yyyy = g[1], mm = g[2], dd = g[3];
      }
      if (g[7]) {
        // parse milliseconds
        while (g[7].length < 3) g[7] = g[7] + '0';
        for (var i = g[7].length; i > 3; i--) g[7] /= 10.0;
        g[7] = Math.round(g[7], 0);
      }
      return new Date(yyyy, mm - 1, dd || 1,
                      g[4] || 0, g[5] || 0, g[6] || 0,
                      g[7] || 0);
    }
    return NaN;
  }


  function zpad(n, width) {
    var s = '' + n;
    while (s.length < width) s = '0' + s;
    return s;
  }


  function dateToStr(d) {
    if (!d) return '';
    return (d.getFullYear() + '-' +
            zpad(d.getMonth() + 1, 2) + '-' +
            zpad(d.getDate(), 2));
  }


  function dateTimeToStr(d) {
    if (!d) return '';
    var msec = d.getMilliseconds();
    return (dateToStr(d) + ' ' +
            zpad(d.getHours(), 2) + ':' +
            zpad(d.getMinutes(), 2) + ':' +
            zpad(d.getSeconds(), 2) +
            (msec ? ('.' + zpad(msec, 3)) : ''));
  }


  function convertTypes(data, types) {
    for (var coli in types) {
      var type = types[coli];
      if (type === T_DATE || type === T_DATETIME) {
        for (var rowi in data) {
          data[rowi][coli] = myParseDate(data[rowi][coli]);
        }
      } else if (type === T_NUM || type === T_BOOL) {
        for (var rowi in data) {
          var v = data[rowi][coli];
          if (v != null && v != '') {
            data[rowi][coli] = v * 1;
          }
        }
      }
    }
  }


  function colNameToColNum(grid, colname) {
    var keycol = (colname == '*') ? 0 : grid.headers.indexOf(colname);
    if (keycol < 0) {
      throw new Error('unknown column name "' + colname + '"');
    }
    return keycol;
  }


  var FUNC_RE = /^(\w+)\((.*)\)$/;
  function keyToColNum(grid, key) {
    var g = FUNC_RE.exec(key);
    if (g) {
      return colNameToColNum(grid, g[2]);
    } else {
      return colNameToColNum(grid, key);
    }
  }


  function _groupByLoop(ingrid, keys, initval, addcols_func, putvalues_func) {
    var outgrid = {headers: [], data: [], types: []};
    var keycols = [];
    for (var keyi in keys) {
      var colnum = keyToColNum(ingrid, keys[keyi]);
      keycols.push(colnum);
      outgrid.headers.push(ingrid.headers[colnum]);
      outgrid.types.push(ingrid.types[colnum]);
    }

    addcols_func(outgrid);

    var out = {};
    for (var rowi in ingrid.data) {
      var row = ingrid.data[rowi];
      var key = [];
      for (var kcoli in keycols) {
        key.push(row[keycols[kcoli]]);
      }
      var orow = out[key];
      if (!orow) {
        orow = [];
        for (var keyi in keys) {
          orow[keyi] = row[keycols[keyi]];
        }
        for (var i = keys.length; i < outgrid.headers.length; i++) {
          orow[i] = initval;
        }
        out[key] = orow;
        // deliberately preserve sequencing as much as possible.  The first
        // time we see a given key is when we add it to the outgrid.
        outgrid.data.push(orow);
      }
      putvalues_func(outgrid, key, orow, row);
    }
    return outgrid;
  }


  var colormap = {};
  var next_color = 0;


  var agg_funcs = {
    first: function(l) {
      return l[0];
    },

    last: function(l) {
      return l.slice(l.length - 1)[0];
    },

    only: function(l) {
      if (l.length == 1) {
        return l[0];
      } else if (l.length < 1) {
        return null;
      } else {
        throw new Error('cell has more than one value: only(' + l + ')');
      }
    },

    min: function(l) {
      var out = null;
      for (var i in l) {
        if (out == null || l[i] < out) {
          out = l[i];
        }
      }
      return out;
    },

    max: function(l) {
      var out = null;
      for (var i in l) {
        if (out == null || l[i] > out) {
          out = l[i];
        }
      }
      return out;
    },

    cat: function(l) {
      return l.join(' ');
    },

    count: function(l) {
      return l.length;
    },

    count_nz: function(l) {
      var acc = 0;
      for (var i in l) {
        if (l[i] != null && l[i] != 0) {
          acc++;
        }
      }
      return acc;
    },

    count_distinct: function(l) {
      var a = {};
      for (var i in l) {
        a[l[i]] = 1;
      }
      var acc = 0;
      for (var i in a) {
        acc += 1;
      }
      return acc;
    },

    sum: function(l) {
      var acc;
      if (l.length) acc = 0;
      for (var i in l) {
        acc += parseFloat(l[i]) || 0;
      }
      return acc;
    },

    avg: function(l) {
      return agg_funcs.sum(l) / agg_funcs.count_nz(l);
    },

    // also works for non-numeric values, as long as they're sortable
    median: function(l) {
      var comparator = function(a, b) {
        a = a || '0'; // ensure consistent ordering given NaN and undefined
        b = b || '0';
        if (a < b) {
          return -1;
        } else if (a > b) {
          return 1;
        } else {
          return 0;
        }
      };
      if (l.length > 0) {
        l.sort(comparator);
        return l[parseInt(l.length / 2)];
      } else {
        return null;
      }
    },

    stddev: function(l) {
      var avg = agg_funcs.avg(l);
      var sumsq = 0.0;
      for (var i in l) {
        var d = parseFloat(l[i]) - avg;
        if (d) sumsq += d * d;
      }
      return Math.sqrt(sumsq);
    },

    color: function(l) {
      for (var i in l) {
        var v = l[i];
        if (!(v in colormap)) {
          colormap[v] = ++next_color;
        }
        return colormap[v];
      }
    }
  };
  agg_funcs.count.return_type = T_NUM;
  agg_funcs.count_nz.return_type = T_NUM;
  agg_funcs.count_distinct.return_type = T_NUM;
  agg_funcs.sum.return_type = T_NUM;
  agg_funcs.avg.return_type = T_NUM;
  agg_funcs.stddev.return_type = T_NUM;
  agg_funcs.cat.return_type = T_STRING;
  agg_funcs.color.return_type = T_NUM;


  function groupBy(ingrid, keys, values) {
    // add one value column for every column listed in values.
    var valuecols = [];
    var valuefuncs = [];
    var addcols_func = function(outgrid) {
      for (var valuei in values) {
        var g = FUNC_RE.exec(values[valuei]);
        var field, func;
        if (g) {
          func = agg_funcs[g[1]];
          if (!func) {
            throw new Error('unknown aggregation function "' + g[1] + '"');
          }
          field = g[2];
        } else {
          func = null;
          field = values[valuei];
        }
        var colnum = keyToColNum(ingrid, field);
        if (!func) {
          if (ingrid.types[colnum] === T_NUM ||
              ingrid.types[colnum] === T_BOOL) {
            func = agg_funcs.sum;
          } else {
            func = agg_funcs.count;
          }
        }
        valuecols.push(colnum);
        valuefuncs.push(func);
		if (g) {
        	outgrid.headers.push(field == '*' ? '_count' : g[1] + ingrid.headers[colnum]);
		} else {
        	outgrid.headers.push(field == '*' ? '_count' : ingrid.headers[colnum]);
		}
        outgrid.types.push(func.return_type || ingrid.types[colnum]);
      }
    };

    // by default, we do a count(*) operation for non-numeric value
    // columns, and sum(*) otherwise.
    var putvalues_func = function(outgrid, key, orow, row) {
      for (var valuei in values) {
        var incoli = valuecols[valuei];
        var outcoli = key.length + parseInt(valuei);
        var cell = row[incoli];
        if (!orow[outcoli]) orow[outcoli] = [];
        if (cell != null) {
          orow[outcoli].push(cell);
        }
      }
    };

    var outgrid = _groupByLoop(ingrid, keys, 0,
                               addcols_func, putvalues_func);

    for (var rowi in outgrid.data) {
      var row = outgrid.data[rowi];
      for (var valuei in values) {
        var outcoli = keys.length + parseInt(valuei);
        var func = valuefuncs[valuei];
        row[outcoli] = func(row[outcoli]);
      }
    }

    return outgrid;
  }


  function pivotBy(ingrid, rowkeys, colkeys, valkeys) {
    // We generate a list of value columns based on all the unique combinations
    // of (values in colkeys)*(column names in valkeys)
    var valuecols = {};
    var colkey_outcols = {};
    var colkey_incols = [];
    for (var coli in colkeys) {
      colkey_incols.push(keyToColNum(ingrid, colkeys[coli]));
    }
    var addcols_func = function(outgrid) {
      for (var rowi in ingrid.data) {
        var row = ingrid.data[rowi];
        var colkey = [];
        for (var coli in colkey_incols) {
          var colnum = colkey_incols[coli];
          colkey.push(stringifiedCol(row[colnum], ingrid.types[colnum]));
        }
        for (var coli in valkeys) {
          var xcolkey = colkey.concat([valkeys[coli]]);
          if (!(xcolkey in colkey_outcols)) {
            // if there's only one valkey (the common case), don't include the
            // name of the old value column in the new column names; it's
            // just clutter.
            var name = valkeys.length > 1 ?
                xcolkey.join(' ') : colkey.join(' ');
            var colnum = rowkeys.length + colkeys.length + parseInt(coli);
            colkey_outcols[xcolkey] = outgrid.headers.length;
            valuecols[xcolkey] = colnum;
            outgrid.headers.push(name);
            outgrid.types.push(ingrid.types[colnum]);
          }
        }
      }
      console.debug('pivot colkey_outcols', colkey_outcols);
      console.debug('pivot valuecols:', valuecols);
    };

    // by the time pivotBy is called, we're guaranteed that there's only one
    // row with a given (rowkeys+colkeys) key, so there is only one value
    // for each value cell.  Thus we don't need to worry about count/sum here;
    // we just assign the values directly as we see them.
    var putvalues_func = function(outgrid, rowkey, orow, row) {
      var colkey = [];
      for (var coli in colkey_incols) {
        var colnum = colkey_incols[coli];
        colkey.push(stringifiedCol(row[colnum], ingrid.types[colnum]));
      }
      for (var coli in valkeys) {
        var xcolkey = colkey.concat([valkeys[coli]]);
        var outcolnum = colkey_outcols[xcolkey];
        var valuecol = valuecols[xcolkey];
        orow[outcolnum] = row[valuecol];
      }
    };

    return _groupByLoop(ingrid, rowkeys, undefined,
                        addcols_func, putvalues_func);
  }


  function stringifiedCol(value, typ) {
    if (typ === T_DATE) {
      return dateToStr(value) || '';
    } else if (typ === T_DATETIME) {
      return dateTimeToStr(value) || '';
    } else {
      return (value + '') || '(none)';
    }
  }


  function stringifiedCols(row, types) {
    var out = [];
    for (var coli in types) {
      out.push(stringifiedCol(row[coli], types[coli]));
    }
    return out;
  }


  function treeJoinKeys(ingrid, nkeys) {
    var outgrid = {
        headers: ['_tree'].concat(ingrid.headers.slice(nkeys)),
        types: [T_STRING].concat(ingrid.types.slice(nkeys)),
        data: []
    };

    for (var rowi in ingrid.data) {
      var row = ingrid.data[rowi];
      var key = row.slice(0, nkeys);
      var newkey = stringifiedCols(row.slice(0, nkeys),
                                   ingrid.types.slice(0, nkeys)).join('|');
      outgrid.data.push([newkey].concat(row.slice(nkeys)));
    }
    return outgrid;
  }


  function finishTree(ingrid, keys) {
    if (keys.length < 1) {
      keys = ['_tree'];
    }
    var outgrid = {headers: ingrid.headers, data: [], types: ingrid.types};
    var keycols = [];
    for (var keyi in keys) {
      keycols.push(keyToColNum(ingrid, keys[keyi]));
    }

    var seen = {};
    var needed = {};
    for (var rowi in ingrid.data) {
      var row = ingrid.data[rowi];
      var key = [];
      for (var keyi in keycols) {
        var keycol = keycols[keyi];
        key.push(row[keycol]);
      }
      seen[key] = 1;
      delete needed[key];
      outgrid.data.push(row);

      var treekey = key.pop().split('|');
      while (treekey.length > 0) {
        treekey.pop();
        var pkey = key.concat([treekey.join('|')]);
        if (pkey in needed || pkey in seen) break;
        needed[pkey] = [treekey.slice(), row];
      }
    }

    var treecol = keycols.pop();
    for (var needkey in needed) {
      var treekey = needed[needkey][0];
      var inrow = needed[needkey][1];
      var outrow = [];
      for (var keycoli in keycols) {
        var keycol = keycols[keycoli];
        outrow[keycol] = inrow[keycol];
      }
      outrow[treecol] = treekey.join('|');
      outgrid.data.push(outrow);
    }

    return outgrid;
  }


  function invertTree(ingrid, key) {
    if (!key) {
      key = '_tree';
    }
    var keycol = keyToColNum(ingrid, key);
    var outgrid = {headers: ingrid.headers, data: [], types: ingrid.types};
    for (var rowi in ingrid.data) {
      var row = ingrid.data[rowi];
      var cell = row[keycol];
      var outrow = row.slice();
      outrow[keycol] = cell.split('|').reverse().join('|');
      outgrid.data.push(outrow);
    }
    return outgrid;
  }


  function crackTree(ingrid, key) {
    if (!key) {
      key = '_tree';
    }
    var keycol = keyToColNum(ingrid, key);
    var outgrid = {
      headers:
        [].concat(ingrid.headers.slice(0, keycol),
                  ['_id', '_parent'],
                  ingrid.headers.slice(keycol + 1)),
      data: [],
      types:
        [].concat(ingrid.types.slice(0, keycol),
                  [T_STRING, T_STRING],
                  ingrid.types.slice(keycol + 1))
    };

    for (var rowi in ingrid.data) {
      var row = ingrid.data[rowi];
      var key = row[keycol];
      var pkey;
      if (!key) {
        key = 'ALL';
        pkey = '';
      } else {
        var keylist = key.split('|');
        keylist.pop();
        pkey = keylist.join('|');
        if (!pkey) {
          pkey = 'ALL';
        }
      }
      outgrid.data.push([].concat(row.slice(0, keycol),
                                  [key, pkey],
                                  row.slice(keycol + 1)));
   }
    return outgrid;
  }


  function splitNoEmpty(s, splitter) {
    if (!s) return [];
    return s.split(splitter);
  }


  function keysOtherThan(grid, keys) {
    var out = [];
    var keynames = [];
    for (var keyi in keys) {
      // this converts func(x) notation to just 'x'
      keynames.push(grid.headers[keyToColNum(grid, keys[keyi])]);
    }
    for (var coli in grid.headers) {
      if (keynames.indexOf(grid.headers[coli]) < 0) {
        out.push(grid.headers[coli]);
      }
    }
    return out;
  }


  function doGroupBy(grid, argval) {
    console.debug('groupBy:', argval);
    var parts = argval.split(';', 2);
    var keys = splitNoEmpty(parts[0], ',');
    var values;
    if (parts.length >= 2) {
      // if there's a ';' separator, the names after it are the desired
      // value columns (and that list may be empty).
      var tmpvalues = splitNoEmpty(parts[1], ',');
      values = [];
      for (var tmpi in tmpvalues) {
        var tmpval = tmpvalues[tmpi];
        if (tmpval == '*') {
          values = values.concat(keysOtherThan(grid, keys.concat(values)));
        } else {
          values.push(tmpval);
        }
      }
    } else {
      // if there is no ';' at all, the default is to just pull in all the
      // remaining non-key columns as values.
      values = keysOtherThan(grid, keys);
    }
    console.debug('grouping by', keys, values);
    grid = groupBy(grid, keys, values);
    console.debug('grid:', grid);
    return grid;
  }


  function doTreeGroupBy(grid, argval) {
    console.debug('treeGroupBy:', argval);
    var parts = argval.split(';', 2);
    var keys = splitNoEmpty(parts[0], ',');
    var values;
    if (parts.length >= 2) {
      // if there's a ';' separator, the names after it are the desired
      // value columns (and that list may be empty).
      values = splitNoEmpty(parts[1], ',');
    } else {
      // if there is no ';' at all, the default is to just pull in all the
      // remaining non-key columns as values.
      values = keysOtherThan(grid, keys);
    }
    console.debug('treegrouping by', keys, values);
    grid = groupBy(grid, keys, values);
    grid = treeJoinKeys(grid, keys.length);
    console.debug('grid:', grid);
    return grid;
  }


  function doFinishTree(grid, argval) {
    console.debug('finishTree:', argval);
    var keys = splitNoEmpty(argval, ',');
    console.debug('finishtree with keys', keys);
    grid = finishTree(grid, keys);
    console.debug('grid:', grid);
    return grid;
  }


  function doInvertTree(grid, argval) {
    console.debug('invertTree:', argval);
    var keys = splitNoEmpty(argval, ',');
    console.debug('invertTree with key', keys[0]);
    grid = invertTree(grid, keys[0]);
    console.debug('grid:', grid);
    return grid;
  }


  function doCrackTree(grid, argval) {
    console.debug('crackTree:', argval);
    var keys = splitNoEmpty(argval, ',');
    console.debug('cracktree with key', keys[0]);
    grid = crackTree(grid, keys[0]);
    console.debug('grid:', grid);
    return grid;
  }


  function doPivotBy(grid, argval) {
    console.debug('pivotBy:', argval);

    // the parts are rowkeys;colkeys;values
    var parts = argval.split(';', 3);
    var rowkeys = splitNoEmpty(parts[0], ',');
    var colkeys = splitNoEmpty(parts[1], ',');
    var values;
    if (parts.length >= 3) {
      // if there's a second ';' separator, the names after it are the desired
      // value columns.
      values = splitNoEmpty(parts[2], ',');
    } else {
      // if there is no second ';' at all, the default is to just pull
      // in all the remaining non-key columns as values.
      values = keysOtherThan(grid, rowkeys.concat(colkeys));
    }

    // first group by the rowkeys+colkeys, so there is only one row for each
    // unique rowkeys+colkeys combination.
    grid = groupBy(grid, rowkeys.concat(colkeys), values);
    console.debug('tmpgrid:', grid);

    // now actually do the pivot.
    grid = pivotBy(grid, rowkeys, colkeys, values);

    return grid;
  }


  function filterBy(ingrid, key, op, values) {
    var outgrid = {headers: ingrid.headers, data: [], types: ingrid.types};
    var keycol = keyToColNum(ingrid, key);
    var wantvals = [];
    for (var valuei in values) {
      if (ingrid.types[keycol] === T_NUM) {
        wantvals.push(parseFloat(values[valuei]));
      } else if (ingrid.types[keycol] === T_DATE ||
                 ingrid.types[keycol] === T_DATETIME) {
        wantvals.push(dateTimeToStr(myParseDate(values[valuei])));
      } else {
        wantvals.push(values[valuei]);
      }
    }

    for (var rowi in ingrid.data) {
      var row = ingrid.data[rowi];
      var cell = row[keycol];
      if (cell == undefined) {
        cell = null;
      }
      var keytype = ingrid.types[keycol];
      if (keytype == T_DATE || keytype == T_DATETIME) {
        cell = dateTimeToStr(cell);
      }
      var found = 0;
      for (var valuei in wantvals) {
        if (op == '=' && cell == wantvals[valuei]) {
          found = 1;
        } else if (op == '==' && cell == wantvals[valuei]) {
          found = 1;
        } else if (op == '>=' && cell >= wantvals[valuei]) {
          found = 1;
        } else if (op == '<=' && cell <= wantvals[valuei]) {
          found = 1;
        } else if (op == '>' && cell > wantvals[valuei]) {
          found = 1;
        } else if (op == '<' && cell < wantvals[valuei]) {
          found = 1;
        } else if (op == '!=' && cell != wantvals[valuei]) {
          found = 1;
        } else if (op == '<>' && cell != wantvals[valuei]) {
          found = 1;
        }
        if (found) break;
      }
      if (found) outgrid.data.push(row);
    }
    return outgrid;
  }


  function trySplitOne(argval, splitstr) {
    var pos = argval.indexOf(splitstr);
    if (pos >= 0) {
      return [argval.substr(0, pos).trim(),
              argval.substr(pos + splitstr.length).trim()];
    } else {
      return;
    }
  }


  function doFilterBy(grid, argval) {
    console.debug('filterBy:', argval);
    var ops = ['>=', '<=', '==', '!=', '<>', '>', '<', '='];
    var parts;
    for (var opi in ops) {
      var op = ops[opi];
      if ((parts = trySplitOne(argval, op))) {
        var matches = parts[1].split(',');
        console.debug('filterBy parsed:', parts[0], op, matches);
        grid = filterBy(grid, parts[0], op, matches);
        console.debug('grid:', grid);
        return grid;
      }
    }
    throw new Error('unknown filter operation in "' + argval + '"');
    return grid;
  }


  function queryBy(ingrid, words) {
    var outgrid = {headers: ingrid.headers, data: [], types: ingrid.types};
    for (var rowi in ingrid.data) {
      var row = ingrid.data[rowi];
      var found = 0, skipped = 0;
      for (var wordi in words) {
        var word = words[wordi];
        if (word[0] == '!' || word[0] == '-') {
          found = 1;
        }
        for (var coli in row) {
          var cell = row[coli];
          if (cell != null && cell.toString().indexOf(word) >= 0) {
            found = 1;
            break;
          } else if ((word[0] == '!' || word[0] == '-') &&
                     (cell != null &&
                      cell.toString().indexOf(word.substr(1)) >= 0)) {
            skipped = 1;
            break;
          }
        }
        if (found || skipped) break;
      }
      if (found && !skipped) {
        outgrid.data.push(row);
      }
    }
    return outgrid;
  }


  function doQueryBy(grid, argval) {
    console.debug('queryBy:', argval);
    grid = queryBy(grid, argval.split(','));
    console.debug('grid:', grid);
    return grid;
  }


  function deltaBy(ingrid, keys) {
    var outgrid = {headers: ingrid.headers, data: [], types: ingrid.types};
    for (var rowi = 0; rowi < ingrid.data.length; rowi++) {
      var row = ingrid.data[rowi];
      outgrid.data.push(row);
    }

    var keycols = [];
    for (var keyi in keys) {
      var key = keys[keyi];
      keycols.push(keyToColNum(ingrid, key));
    }

    if (outgrid.data.length < 2) {
      return outgrid;
    }
    for (var keyi in keycols) {
      var keycol = keycols[keyi];

      var prev_val = undefined;
      for (var rowi = 1; rowi < outgrid.data.length; rowi++) {
        var row = outgrid.data[rowi];
        var val = row[keycol];
        if (val == undefined) {
          continue;
        } else if (outgrid.types[keycol] === T_NUM) {
          if (prev_val != undefined) {
            if (val > prev_val) {
              var new_val = val - prev_val;
              outgrid.data[rowi][keycol] = new_val;
            } else if (val == prev_val) {
              outgrid.data[rowi][keycol] = undefined;
            }
          }
          prev_val = val;
        }
      }
    }

    return outgrid;
  }


  function doDeltaBy(grid, argval) {
    console.debug('deltaBy:', argval);
    grid = deltaBy(grid, argval.split(','));
    console.debug('grid:', grid);
    return grid;
  }


  function unselectBy(ingrid, keys) {
    var outgrid = {headers: [], data: [], types: []};
    var keycols = {};
    for (var keyi in keys) {
      var key = keys[keyi];
      var col = keyToColNum(ingrid, key);
      keycols[col] = true;
    }

    for (var headi = 0; headi < ingrid.headers.length; headi++) {
      if (!(headi in keycols)) {
        outgrid.headers.push(ingrid.headers[headi]);
        outgrid.types.push(ingrid.types[headi]);
      }
    }
    for (var rowi = 0; rowi < ingrid.data.length; rowi++) {
      var row = ingrid.data[rowi];
      var newrow = [];
      for (var coli = 0; coli < row.length; coli++) {
        if (!(coli in keycols)) {
          newrow.push(row[coli]);
        }
      }
      outgrid.data.push(newrow);
    }

    return outgrid;
  }


  function doUnselectBy(grid, argval) {
    console.debug('unselectBy:', argval);
    grid = unselectBy(grid, argval.split(','));
    console.debug('grid:', grid);
    return grid;
  }


  function orderBy(grid, keys) {
    var keycols = [];
    for (var keyi in keys) {
      var key = keys[keyi];
      var invert = 1;
      if (key[0] == '-') {
        invert = -1;
        key = key.substr(1);
      }
      keycols.push([keyToColNum(grid, key), invert]);
    }
    console.debug('sort keycols', keycols);
    var comparator = function(a, b) {
      for (var keyi in keycols) {
        var keycol = keycols[keyi][0], invert = keycols[keyi][1];
        var av = a[keycol], bv = b[keycol];
        if (grid.types[keycol] === T_NUM) {
          av = parseFloat(av);
          bv = parseFloat(bv);
        }
        av = av || '0'; // ensure consistent ordering given NaN and undefined
        bv = bv || '0';
        if (av < bv) {
          return -1 * invert;
        } else if (av > bv) {
          return 1 * invert;
        }
      }
      return 0;
    };
    var outdata = grid.data.concat();
    outdata.sort(comparator);
    return { headers: grid.headers, data: outdata, types: grid.types };
  }


  function doOrderBy(grid, argval) {
    console.debug('orderBy:', argval);
    grid = orderBy(grid, argval.split(','));
    console.debug('grid:', grid);
    return grid;
  }


  function extractRegexp(grid, colname, regexp) {
    var r = RegExp(regexp);
    var colnum = keyToColNum(grid, colname);
    var typ = grid.types[colnum];
    grid.types[colnum] = T_STRING;
    for (var rowi in grid.data) {
      var row = grid.data[rowi];
      var match = r.exec(stringifiedCol(row[colnum], typ));
      if (match) {
        row[colnum] = match.slice(1).join('');
      } else {
        row[colnum] = '';
      }
    }
    return grid;
  }


  function doExtractRegexp(grid, argval) {
    console.debug('extractRegexp:', argval);
    var parts = trySplitOne(argval, '=');
    var colname = parts[0], regexp = parts[1];
    if (regexp.indexOf('(') < 0) {
      throw new Error('extract_regexp should have at least one (regex group)');
    }
    grid = extractRegexp(grid, colname, regexp);
    console.debug('grid:', grid);
    return grid;
  }


  function quantize(grid, colname, quants) {
    var colnum = keyToColNum(grid, colname);
    if (quants.length == 0) {
      throw new Error('quantize needs a bin size or list of edges');
    } else if (quants.length == 1) {
      // they specified a bin size
      var binsize = quants[0] * 1;
      if (binsize <= 0) {
        throw new Error('quantize: bin size ' + binsize + ' must be > 0');
      }
      for (var rowi in grid.data) {
        var row = grid.data[rowi];
        var binnum = Math.floor(row[colnum] / binsize);
        row[colnum] = binnum * binsize;
      }
    } else {
      // they specified the actual bin edges
      for (var rowi in grid.data) {
        var row = grid.data[rowi];
        var val = row[colnum];
        var out = undefined;
        for (var quanti in quants) {
          if (val * 1 < quants[quanti] * 1) {
            if (quanti == 0) {
              out = '<' + quants[0];
              break;
            } else {
              out = quants[quanti - 1] + '-' + quants[quanti];
              break;
            }
          }
        }
        if (!out) out = quants[quants.length - 1] + '+';
        row[colnum] = out;
      }
    }
    return grid;
  }


  function doQuantize(grid, argval) {
    console.debug('quantize:', argval);
    var parts = trySplitOne(argval, '=');
    var colname = parts[0], quants = parts[1].split(',');
    grid = quantize(grid, colname, quants);
    console.debug('grid:', grid);
    return grid;
  }


  function yspread(grid) {
    for (var rowi in grid.data) {
      var row = grid.data[rowi];
      var total = 0;
      for (var coli in row) {
        if (grid.types[coli] == T_NUM && row[coli]) {
          total += Math.abs(row[coli] * 1);
        }
      }
      if (!total) total = 1;
      for (var coli in row) {
        if (grid.types[coli] == T_NUM && row[coli]) {
          row[coli] = row[coli] * 1 / total;
        }
      }
    }
    return grid;
  }


  function doYSpread(grid, argval) {
    console.debug('yspread:', argval);
    if (argval) {
      throw new Error('yspread: no argument expected');
    }
    grid = yspread(grid);
    console.debug('grid:', grid);
    return grid;
  }


  function doLimit(ingrid, limit) {
    limit = parseInt(limit);
    if (ingrid.data.length > limit) {
      return {
          headers: ingrid.headers,
          data: ingrid.data.slice(0, limit),
          types: ingrid.types
      };
    } else {
      return ingrid;
    }
  }


  function limitDecimalPrecision(grid) {
    for (var rowi in grid.data) {
      var row = grid.data[rowi];
      for (var coli in row) {
        var cell = row[coli];
        if (cell === +cell) {
          row[coli] = parseFloat(cell.toPrecision(15));
        }
      }
    }
    return grid;
  }


  function fillNullsWithZero(grid) {
    var fcount = 0;
    for (var rowi in grid.data) {
      var row = grid.data[rowi];
      for (var coli in row) {
        if (grid.types[coli] === T_NUM && row[coli] == undefined) {
          row[coli] = 0;
		  fcount++;
        }
        if (grid.types[coli] === T_STRING && row[coli] == undefined) {
          row[coli] = "_undefined_";
		  fcount++;
        }
      }
    }
    console.debug('fillNullsWithZero filled:', fcount);
    return grid;
  }


  function isString(v) {
    return v.charAt !== undefined;
  }


  function isArray(v) {
    return v.splice !== undefined;
  }


  function isObject(v) {
    return typeof(v) === 'object';
  }


  function isDate(v) {
    return v.getDate !== undefined;
  }


  function isScalar(v) {
    return v == undefined || isString(v) || !isObject(v) || isDate(v);
  }


  function check2d(rawdata) {
    if (!isArray(rawdata)) return false;
    for (var rowi = 0; rowi < rawdata.length && rowi < 5; rowi++) {
      var row = rawdata[rowi];
      if (!isArray(row)) return false;
      for (var coli = 0; coli < row.length; coli++) {
        var col = row[coli];
        if (!isScalar(col)) return false;
      }
    }
    return true;
  }


  function _copyObj(out, v) {
    for (var key in v) {
      out[key] = v[key];
    }
    return out;
  }


  function copyObj(v) {
    return _copyObj({}, v);
  }


  function multiplyLists(out, l1, l2) {
    if (l1 === undefined) throw new Error('l1 undefined');
    if (l2 === undefined) throw new Error('l2 undefined');
    for (var l1i in l1) {
      var r1 = l1[l1i];
      for (var l2i in l2) {
        var r2 = l2[l2i];
        var o = copyObj(r1);
        _copyObj(o, r2);
        out.push(o);
      }
    }
    return out;
  }


  function flattenDict(headers, coldict, rowtmp, d) {
    var out = [];
    var lists = [];
    for (var key in d) {
      var value = d[key];
      if (isScalar(value)) {
        if (coldict[key] === undefined) {
          coldict[key] = headers.length;
          headers.push(key);
        }
        rowtmp[key] = value;
      } else if (isArray(value)) {
        lists.push(flattenList(headers, coldict, value));
      } else {
        lists.push(flattenDict(headers, coldict, rowtmp, value));
      }
    }

    // now multiply all the lists together
    var tmp1 = [{}];
    while (lists.length) {
      var tmp2 = [];
      multiplyLists(tmp2, tmp1, lists.shift());
      tmp1 = tmp2;
    }

    // this is apparently the "right" way to append a list to a list.
    Array.prototype.push.apply(out, tmp1);
    return out;
  }


  function flattenList(headers, coldict, rows) {
    var out = [];
    for (var rowi in rows) {
      var row = rows[rowi];
      var rowtmp = {};
      var sublist = flattenDict(headers, coldict, rowtmp, row);
      multiplyLists(out, [rowtmp], sublist);
    }
    return out;
  }


  function gridFromData(rawdata) {
    if (rawdata && rawdata.headers && rawdata.data && rawdata.types) {
      // already in grid format
      return rawdata;
    }

    var headers, data, types;

    var err;
    if (rawdata.errors && rawdata.errors.length) {
      err = rawdata.errors[0];
    } else if (rawdata.error) {
      err = rawdata.error;
    }
    if (err) {
      var msglist = [];
      if (err.message) msglist.push(err.message);
      if (err.detailed_message) msglist.push(err.detailed_message);
      throw new Error('Data provider returned an error: ' + msglist.join(': '));
    }

    if (rawdata.table) {
      // gviz format
      headers = [];
      for (var headeri in rawdata.table.cols) {
        headers.push(rawdata.table.cols[headeri].label ||
                     rawdata.table.cols[headeri].id);
      }
      data = [];
      for (var rowi in rawdata.table.rows) {
        var row = rawdata.table.rows[rowi];
        var orow = [];
        for (var coli in row.c) {
          var col = row.c[coli];
          var g;
          if (!col) {
            orow.push(null);
          } else {
            orow.push(col.v);
          }
        }
        data.push(orow);
      }
    } else if (rawdata.data && rawdata.cols) {
      // eqldata.com format
      headers = [];
      for (var coli in rawdata.cols) {
        var col = rawdata.cols[coli];
        headers.push(col.caption || col);
      }
      data = rawdata.data;
    } else if (check2d(rawdata)) {
      // simple [[cols...]...] (two-dimensional array) format, where
      // the first row is the headers.
      headers = rawdata[0];
      data = rawdata.slice(1);
    } else if (isArray(rawdata)) {
      // assume datacube format, which is a nested set of lists and dicts.
      // A dict contains a list of key (column name) and value (cell content)
      // pairs.  A list contains a set of lists or dicts, which corresponds
      // to another "dimension" of the cube.  To flatten it, we have to
      // replicate the row once for each element in the list.
      var coldict = {};
      headers = [];
      var rowdicts = flattenList(headers, coldict, rawdata);
      data = [];
      for (var rowi in rowdicts) {
        var rowdict = rowdicts[rowi];
        var row = [];
        for (var coli in headers) {
          row.push(rowdict[headers[coli]]);
        }
        data.push(row);
      }
    } else {
      throw new Error("don't know how to parse this json layout, sorry!");
    }
    types = guessTypes(data);
    convertTypes(data, types);
    return {headers: headers, data: data, types: types};
  }


  function enqueue(queue, stepname, func) {
    queue.push([stepname, func]);
  }


  function runqueue(queue, ingrid, done, showstatus, wrap_each, after_each) {
    var step = function(i) {
      if (i < queue.length) {
        var el = queue[i];
        var text = el[0], func = el[1];
        if (showstatus) {
          showstatus('Running step ' + (+i + 1) + ' of ' +
                     queue.length + '...',
                     text);
        }
        setTimeout(function() {
          var start = Date.now();
          var wfunc = wrap_each ? wrap_each(func) : func;
          wfunc(ingrid, function(outgrid) {
            var end = Date.now();
            if (after_each) {
              after_each(outgrid, i + 1, queue.length, text, end - start);
            }
            ingrid = outgrid;
            step(i + 1);
          });
        }, 0);
      } else {
        if (showstatus) {
          showstatus('');
        }
        if (done) {
          done(ingrid);
        }
      }
    };
    step(0);
  }


  function maybeSet(dict, key, value) {
    if (!(key in dict)) {
      dict[key] = value;
    }
  }


  function addTransforms(queue, args) {
    var trace = args.get('trace');
    var argi;

    // helper function for synchronous transformations (ie. ones that return
    // the output grid rather than calling a callback)
    var transform = function(f, arg) {
      enqueue(queue, args.all[argi][0] + '=' + args.all[argi][1],
              function(ingrid, done) {
        var outgrid = f(ingrid, arg);
        done(outgrid);
      });
    };

    for (var argi in args.all) {
      var argkey = args.all[argi][0], argval = args.all[argi][1];
      if (argkey == 'group') {
        transform(doGroupBy, argval);
      } else if (argkey == 'treegroup') {
        transform(doTreeGroupBy, argval);
      } else if (argkey == 'finishtree') {
        transform(doFinishTree, argval);
      } else if (argkey == 'inverttree') {
        transform(doInvertTree, argval);
      } else if (argkey == 'cracktree') {
        transform(doCrackTree, argval);
      } else if (argkey == 'pivot') {
        transform(doPivotBy, argval);
      } else if (argkey == 'filter') {
        transform(doFilterBy, argval);
      } else if (argkey == 'q') {
        transform(doQueryBy, argval);
      } else if (argkey == 'limit') {
        transform(doLimit, argval);
      } else if (argkey == 'delta') {
        transform(doDeltaBy, argval);
      } else if (argkey == 'unselect') {
        transform(doUnselectBy, argval);
      } else if (argkey == 'order') {
        transform(doOrderBy, argval);
      } else if (argkey == 'extract_regexp') {
        transform(doExtractRegexp, argval);
      } else if (argkey == 'quantize') {
        transform(doQuantize, argval);
      } else if (argkey == 'yspread') {
        transform(doYSpread, argval);
      }
    }
  }


  function dyIndexFromX(dychart, x) {
    // TODO(apenwarr): consider a binary search
    for (var i = dychart.numRows() - 1; i >= 0; i--) {
      if (dychart.getValue(i, 0) <= x) {
        break;
      }
    }
    return i;
  }


  function createTracesChart(grid, el, colsPerChart) {
    var charts = [];
    var xlines = [];

    var n = (grid.headers.length - 1) / colsPerChart;
    $(el).css('overflow', 'scroll');
    var tab = $('<table class="dy-table" style="width:100%;"/>');
    $(el).append(tab);

    var zooming = false;
    for (var i = 0; i < n; i++) {
      var dyoptions = {
        connectSeparatedPoints: false,
        drawYAxis: true,
        drawXAxis: false,
        drawGrid: false,
        drawPoints: true,
        fillGraph: false,
        hideOverlayOnMouseOut: false,
        stepPlot: true,
        strokeWidth: 0,
        zoomCallback: function(x1, x2, yranges) {
          if (zooming) return;
          zooming = true;
          var range = [x1, x2];
          for (var charti = 0; charti < charts.length; charti++) {
            charts[charti].updateOptions({ dateWindow: range });
          }
          zooming = false;
        }
      };
      var coli = 1 + (i * colsPerChart);
      var row = $('<tr>');
      var labelcol = $('<td>').text(grid.headers[coli]);
      var col = $('<td style="width:100%">');
      $(tab).append(row);
      $(row).append(labelcol);
      $(row).append(col);
      var odd = (i % 2) ? 'odd' : 'even';
      var sub = $('<div class="dy-chart dy-chart-' + odd + '"/>')[0];
      $(sub).css('position', 'relative');
      $(col).append(sub);
      dyoptions.labels = [grid.headers[0], 'value'];

      var data = [];
      if (colsPerChart == 3) {
        // input data is x,val,min,max
        // output data must be x,[min,val,max]
        for (var rowi = 0; rowi < grid.data.length; rowi++) {
          var datacol = [grid.data[rowi][coli + 1],
                         grid.data[rowi][coli + 0],
                         grid.data[rowi][coli + 2]];
          if (rowi == 0 ||
              rowi == grid.data.length - 1 ||
              datacol[0] != null ||
              datacol[1] != null ||
              datacol[2] != null) {
            data.push([grid.data[rowi][0], datacol]);
          }
        }
      } else {
        for (var rowi = 0; rowi < grid.data.length; rowi++) {
          if (rowi == 0 ||
              rowi == grid.data.length - 1 ||
              grid.data[rowi][coli] != null) {
            data.push([grid.data[rowi][0], grid.data[rowi][coli]]);
          }
        }
      }

      if (i == n - 1) {
        $(sub).css('height', '140px');
        dyoptions.drawXAxis = true;
      } else {
        $(sub).css('height', '120px');
      }

      var xline = $('<div class="dy-line dy-xline" />')[0];
      var yline = $('<div class="dy-line dy-yline" />')[0];
      var inHighlighter = false;
      var declareCallback = function(data, xline, yline) {
        return function(graph, series, canvas, x, y, color, size, idx) {
          $(xline).css('left', x - 1 + 'px');
          $(yline).css('top', y - 1 + 'px');

          // also move the highlight in all other charts
          if (inHighlighter) return;
          inHighlighter = true;
          for (var charti = 0; charti < charts.length; charti++) {
            var otheridx = dyIndexFromX(charts[charti], data[idx][0]);
            charts[charti].setSelection(otheridx);
          }
          inHighlighter = false;
        };
      };
      dyoptions.drawHighlightPointCallback = declareCallback(data,
                                                             xline, yline);
      if (colsPerChart == 3) {
        dyoptions.customBars = true;
      } else {
        dyoptions.customBars = false;
      }

      var chart = new Dygraph(sub, data, dyoptions);
      $(sub).append(xline, yline);
      charts.push(chart);
    }

    return { draw: function(table, options) { } };
  }


  function addRenderers(queue, args) {
    var trace = args.get('trace');
    var chartops = args.get('chart');
    var t, datatable, resizeTimer;
    var options = {};
    var intensify = args.get('intensify');
    if (intensify == '') {
      intensify = 'xy';
    }
    var gridoptions = {
      intensify: intensify
    };
    var el = document.getElementById('vizchart');

    enqueue(queue, 'gentable', function(grid, done) {
      grid = fillNullsWithZero(grid);
      if (chartops) {
        var chartbits = chartops.split(',');
        var charttype = chartbits.shift();
        for (var charti in chartbits) {
          var kv = trySplitOne(chartbits[charti], '=');
          options[kv[0]] = kv[1];
        }
        if (charttype == 'stacked' || charttype == 'stackedarea') {
          // Some charts react badly to missing values, so fill them in.
          grid = fillNullsWithZero(grid);
        }
        grid = limitDecimalPrecision(grid);

        // Scan and add all args not for afterquery to GViz options.
        scanGVizChartOptions(args, options);

        if (charttype == 'stackedarea' || charttype == 'stacked') {
          t = new google.visualization.AreaChart(el);
          options.isStacked = true;
		  options.explorer = {};
        } else if (charttype == 'column') {
          t = new google.visualization.ColumnChart(el);
        } else if (charttype == 'bar') {
          t = new google.visualization.BarChart(el);
        } else if (charttype == 'line') {
          t = new google.visualization.LineChart(el);
        } else if (charttype == 'spark') {
          // sparkline chart: get rid of everything but the data series.
          // Looks best when small.
          options.hAxis = {};
          options.hAxis.baselineColor = 'none';
          options.hAxis.textPosition = 'none';
          options.hAxis.gridlines = {};
          options.hAxis.gridlines.color = 'none';
          options.vAxis = {};
          options.vAxis.baselineColor = 'none';
          options.vAxis.textPosition = 'none';
          options.vAxis.gridlines = {};
          options.vAxis.gridlines.color = 'none';
          options.theme = 'maximized';
          options.legend = {};
          options.legend.position = 'none';
          t = new google.visualization.LineChart(el);
        } else if (charttype == 'pie') {
          t = new google.visualization.PieChart(el);
        } else if (charttype == 'tree') {
          if (grid.headers[0] == '_tree') {
            grid = finishTree(grid, ['_tree']);
            grid = crackTree(grid, '_tree');
          }
          maybeSet(options, 'maxDepth', 3);
          maybeSet(options, 'maxPostDepth', 1);
          maybeSet(options, 'showScale', 1);
          t = new google.visualization.TreeMap(el);
        } else if (charttype == 'candle' || charttype == 'candlestick') {
          t = new google.visualization.CandlestickChart(el);
        } else if (charttype == 'timeline') {
          t = new google.visualization.AnnotatedTimeLine(el);
        } else if (charttype == 'dygraph' || charttype == 'dygraph+errors') {
          t = new Dygraph.GVizChart(el);
          maybeSet(options, 'showRoller', true);
          if (charttype == 'dygraph+errors') {
            options.errorBars = true;
          }
        } else if (charttype == 'traces' || charttype == 'traces+minmax') {
          t = createTracesChart(grid, el,
                                charttype == 'traces+minmax' ? 3 : 1);
        } else if (charttype == 'heatgrid') {
          t = new HeatGrid(el);
        } else {
          throw new Error('unknown chart type "' + charttype + '"');
        }
        gridoptions.show_only_lastseg = true;
        gridoptions.bool_to_num = true;
      } else {
        t = new google.visualization.Table(el);
        gridoptions.allowHtml = true;
        options.allowHtml = true;
      }

      if (charttype == 'heatgrid' ||
          charttype == 'traces' ||
          charttype == 'traces+minmax') {
        datatable = grid;
      } else {
        datatable = dataToGvizTable(grid, gridoptions);

        var dateformat = new google.visualization.DateFormat({
          pattern: 'yyyy-MM-dd'
        });
        var datetimeformat = new google.visualization.DateFormat({
          pattern: 'yyyy-MM-dd HH:mm:ss'
        });
        for (var coli = 0; coli < grid.types.length; coli++) {
          if (grid.types[coli] === T_DATE) {
            dateformat.format(datatable, coli);
          } else if (grid.types[coli] === T_DATETIME) {
            datetimeformat.format(datatable, coli);
          }
        }
      }
      done(grid);
    });

    enqueue(queue, chartops ? 'chart=' + chartops : 'view',
            function(grid, done) {
      if (grid.data.length) {
        var doRender = function() {
          var wantwidth = trace ? window.innerWidth - 40 : window.innerWidth;
          $(el).width(wantwidth);
          $(el).height(window.innerHeight);
          options.height = window.innerHeight;
          t.draw(datatable, options);
        };
        doRender();
        $(window).resize(function() {
          clearTimeout(resizeTimer);
          resizeTimer = setTimeout(doRender, 200);
        });
      } else {
        el.innerHTML = 'Empty dataset.';
      }
      done(grid);
    });
  }

  function scanGVizChartOptions(args, options) {
    // Parse args to be sent to GViz.
    var allArgs = args['all'];
    for (var i in allArgs) {
      var key = allArgs[i][0];
      // Ignore params sent for afterquery.
      if (key == 'trace'
          || key == 'url'
          || key == 'chart'
          || key == 'editlink'
          || key == 'intensify'
          || key == 'order'
          || key == 'group'
          || key == 'limit'
          || key == 'filter'
          || key == 'q'
          || key == 'pivot'
          || key == 'treegroup'
          || key == 'extract_regexp') {
        continue;
      }
      // Add params for GViz API into options object.
      addGVizChartOption(options, key, allArgs[i][1]);
    }
    console.debug('Options sent to GViz');
    console.debug(options);
  }

  function addGVizChartOption(options, key, value) {
    if (key.indexOf('.') > -1) {
      var subObjects = key.split('.');
      if (!options[subObjects[0]]) {
        options[subObjects[0]] = {};
      }
      addGVizChartOption(options[subObjects[0]],
        key.substring(key.indexOf('.') + 1), value);
    } else {
      options[key] = value;
    }
  }

  function finishQueue(queue, args, done) {
    var trace = args.get('trace');
    if (trace) {
      var prevdata;
      var after_each = function(grid, stepi, nsteps, text, msec_time) {
        $('#vizlog').append('<div class="vizstep" id="step' + stepi + '">' +
                            '  <div class="text"></div>' +
                            '  <div class="grid"></div>' +
                            '</div>');
        $('#step' + stepi + ' .text').text('Step ' + stepi +
                                           ' (' + msec_time + 'ms):  ' +
                                           text);
        var viewel = $('#step' + stepi + ' .grid');
        if (prevdata != grid.data) {
          var t = new google.visualization.Table(viewel[0]);
          var datatable = dataToGvizTable({
            headers: grid.headers,
            data: grid.data.slice(0, 1000),
            types: grid.types
          });
          t.draw(datatable);
          prevdata = grid.data;
        } else {
          viewel.text('(unchanged)');
        }
        if (stepi == nsteps) {
          $('.vizstep').show();
        }
      };
      runqueue(queue, null, done, showstatus, wrap, after_each);
    } else {
      runqueue(queue, null, done, showstatus, wrap);
    }
  }


  function gotError(url, jqxhr, status) {
    showstatus('');
    $('#vizraw').html('<a href="' + encodeURI(url) + '">' +
                      encodeURI(url) +
                      '</a>');
    throw new Error('error getting url "' + url + '": ' +
                    status + ': ' +
                    'visit the data page and ensure it\'s valid jsonp.');
  }


  function argsToArray(args) {
    // call Array's slice() function on an 'arguments' structure, which is
    // like an array but missing functions like slice().  The result is a
    // real Array object, which is more useful.
    return [].slice.apply(args);
  }


  function wrap(func) {
    // pre_args is the arguments as passed at wrap() time
    var pre_args = argsToArray(arguments).slice(1);
    var f = function() {
      try {
        // post_args is the arguments as passed when calling f()
        var post_args = argsToArray(arguments);
        return func.apply(null, pre_args.concat(post_args));
      } catch (e) {
        $('#vizchart').hide();
        $('#vizstatus').css('position', 'relative');
        $('.vizstep').show();
        err(e);
        err("<p><a href='/help'>here's the documentation</a>");
        throw e;
      }
    };
    return f;
  }


  var URL_RE = RegExp('^((\\w+:)?(//[^/]*)?)');


  function urlMinusPath(url) {
    var g = URL_RE.exec(url);
    if (g && g[1]) {
      return g[1];
    } else {
      return url;
    }
  }


  function checkUrlSafety(url) {
    if (/[<>"''"]/.exec(url)) {
      throw new Error('unsafe url detected. encoded=' + encodedURI(url));
    }
  }


  function extendDataUrl(url) {
    // some services expect callback=, some expect jsonp=, so supply both
    var plus = 'callback=jsonp&jsonp=jsonp';
    var hostpart = urlMinusPath(url);
    var auth = localStorage[['auth', hostpart]];
    if (auth) {
      plus += '&auth=' + encodeURIComponent(auth);
    }

    if (url.indexOf('?') >= 0) {
      return url + '&' + plus;
    } else {
      return url + '?' + plus;
    }
  }


  function extractJsonFromJsonp(text, success_func) {
    var data = text.trim();
    var start = data.indexOf('jsonp(');
    if (start >= 0) {
      data = data.substr(start + 6, data.length - start - 6 - 2);
    }
    console.debug('in extractJsonFromJsonp start: ', start);
    data = JSON.parse(data);
    success_func(data);
  }


  function getUrlData_xhr(url, success_func, error_func) {
    jQuery.support.cors = true;
    jQuery.ajax(url, {
      headers: { 'X-DataSource-Auth': 'a' },
      xhrFields: { withCredentials: true },
      dataType: 'text',
      success: function(text) { extractJsonFromJsonp(text, success_func); },
      error: error_func
    });
  }


  function getUrlData_jsonp(url, success_func, error_func) {
    var iframe = document.createElement('iframe');
    iframe.style.display = 'none';

    iframe.onload = function() {
      var successfunc_called;
      var real_success_func = function(data) {
        console.debug('calling success_func');
        success_func(data);
        successfunc_called = true;
      };

      // the default jsonp callback
      iframe.contentWindow.jsonp = real_success_func;

      // a callback the jsonp can execute if oauth2 authentication is needed
      iframe.contentWindow.tryOAuth2 = function(oauth2_url) {
        var hostpart = urlMinusPath(oauth2_url);
        var oauth_appends = {
          'https://accounts.google.com':
              'client_id=41470923181.apps.googleusercontent.com'
          // (If you register afterquery with any other API providers, add the
          //  app ids here.  app client_id fields are not secret in oauth2;
          //  there's a client_secret, but it's not needed in pure javascript.)
        };
        var plus = [oauth_appends[hostpart]];
        if (plus) {
          plus += '&response_type=token';
          plus += '&state=' +
              encodeURIComponent(
                  'url=' + encodeURIComponent(url) +
                  '&continue=' + encodeURIComponent(window.top.location));
          plus += '&redirect_uri=' +
              encodeURIComponent(window.location.origin + '/oauth2callback');
          var want_url;
          if (oauth2_url.indexOf('?') >= 0) {
            want_url = oauth2_url + '&' + plus;
          } else {
            want_url = oauth2_url + '?' + plus;
          }
          console.debug('oauth2 redirect:', want_url);
          checkUrlSafety(want_url);
          document.write('Click here to ' +
                         '<a target="_top" ' +
                         '  href="' + want_url +
                         '">authorize the data source</a>.');
        } else {
          console.debug('no oauth2 service known for host', hostpart);
          document.write("Data source requires authorization, but I don't " +
                         'know how to oauth2 authorize urls from <b>' +
                         encodeURI(hostpart) +
                         '</b> - sorry.');
        }
        // needing oauth2 is "success" in that we know what's going on
        successfunc_called = true;
      };

      // some services are hardcoded to use the gviz callback, so
      // supply that too
      iframe.contentWindow.google = {
        visualization: {
          Query: {
            setResponse: real_success_func
          }
        }
      };

      iframe.contentWindow.onerror = function(message, xurl, lineno) {
        err(message + ' url=' + xurl + ' line=' + lineno);
      };

      iframe.contentWindow.onpostscript = function() {
        if (successfunc_called) {
          console.debug('json load was successful.');
        } else {
          err('Error loading data; check javascript console for details.');
          err('<a href="' + encodeURI(url) + '">' + encodeURI(url) + '</a>');
        }
      };

      iframe.contentWindow.jsonp_url = url;

      //TODO(apenwarr): change the domain/origin attribute of the iframe.
      //  That way the script won't be able to affect us, no matter how badly
      //  behaved it might be.  That's important so they can't access our
      //  localStorage, set cookies, etc.  We can use the new html5 postMessage
      //  feature to safely send json data from the iframe back to us.
      // ...but for the moment we have to trust the data provider.
      //TODO(apenwarr): at that time, make this script.async=1
      //  ...and run postscript from there.

      var script = iframe.contentDocument.createElement('script');
      script.async = false;
      script.src = url;
      iframe.contentDocument.body.appendChild(script);

      var postscript = iframe.contentDocument.createElement('script');
      postscript.async = false;
      postscript.src = 'postscript.js';
      iframe.contentDocument.body.appendChild(postscript);
    };
    document.body.appendChild(iframe);
  }


  function getUrlData(url, success_func, error_func) {
    console.debug('fetching data url:', url);
    var onError = function(xhr, msg) {
      console.debug('xhr returned error:', msg);
      console.debug('(trying jsonp instead)');
      getUrlData_jsonp(url, success_func, error_func);
    };
    getUrlData_xhr(url, success_func, onError);
  }


  function addUrlGetters(queue, args, startdata) {
    if (!startdata) {
      var url = args.get('url');
      console.debug('original data url:', url);
      if (!url) throw new Error('Missing url= in query parameter');
      if (url.indexOf('//') == 0) url = window.location.protocol + url;
      url = extendDataUrl(url);
      showstatus('Loading <a href="' + encodeURI(url) + '">data</a>...');

      enqueue(queue, 'get data', function(_, done) {
        getUrlData(url, wrap(done), wrap(gotError, url));
      });
    } else {
      enqueue(queue, 'init data', function(_, done) {
        done(startdata);
      });
    }

    enqueue(queue, 'parse', function(rawdata, done) {
      console.debug('rawdata:', rawdata);
      var outgrid = gridFromData(rawdata);
      console.debug('grid:', outgrid);
      done(outgrid);
    });
  }


  function exec(query, startdata, done) {
    var args = parseArgs(query);
    var queue = [];
    addUrlGetters(queue, args, startdata);
    addTransforms(queue, args);
    runqueue(queue, startdata, done);
  }


  function render(query, startdata, done) {
    var args = parseArgs(query);
    var editlink = args.get('editlink');
    if (editlink == 0) {
      $('#editmenu').hide();
    }

    var queue = [];
    addUrlGetters(queue, args, startdata);
    addTransforms(queue, args);
    addRenderers(queue, args);
    finishQueue(queue, args, done);
  }


  return {
    internal: {
      trySplitOne: trySplitOne,
      dataToGvizTable: dataToGvizTable,
      guessTypes: guessTypes,
      myParseDate: myParseDate,
      groupBy: groupBy,
      pivotBy: pivotBy,
      stringifiedCols: stringifiedCols,
      filterBy: filterBy,
      queryBy: queryBy,
      deltaBy: deltaBy,
      unselectBy: unselectBy,
      orderBy: orderBy,
      extractRegexp: extractRegexp,
      fillNullsWithZero: fillNullsWithZero,
      urlMinusPath: urlMinusPath,
      checkUrlSafety: checkUrlSafety,
      argsToArray: argsToArray,
      enqueue: enqueue,
      runqueue: runqueue,
      gridFromData: gridFromData
    },
    T_NUM: T_NUM,
    T_DATE: T_DATE,
    T_DATETIME: T_DATETIME,
    T_BOOL: T_BOOL,
    T_STRING: T_STRING,
    parseArgs: parseArgs,
    exec: exec,
    render: wrap(render)
  };
})();
