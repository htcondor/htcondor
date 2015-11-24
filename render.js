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
 * Additional modifications 2015 by Todd Tannenbaum and Alan De Smet, 
 * Center for High Throughput Computing, University of Wisconsin - Madison.
 * Licensed under the Apache License, Version 2.0.
 */
'use strict';

function AfterqueryObj(options) {
  if(options === null || options === undefined) {
    options = {};
  }

  this.root_id = options.root_id;

  this.colormap = {};
  this.next_color = 0;
};


AfterqueryObj.prototype.elid = function(id) {
    if(this.root_id) {

      return "#"+this.root_id+" ."+id;
    }
    return "#"+id;
}

AfterqueryObj.prototype.err = function(s) {
    $(this.elid("vizlog")).append('\n' + s);
  };


AfterqueryObj.prototype.showstatus = function(s, s2) {
    $(this.elid('statustext')).html(s);
    $(this.elid('statussub')).text(s2 || '');
    if (s || s2) {
      AfterqueryObj.log('status message:', s, s2);
      $(this.elid('vizstatus')).show();
    } else {
      $(this.elid('vizstatus')).hide();
    }
  };

AfterqueryObj.log = function() {
    if(0) {
        var args = ["render.js:"]; 
        for(var i = 0; i < arguments.length; i++) { args.push(arguments[i]); }
        console.log.apply(console, args);
    }
}

AfterqueryObj.parseArgs = function(query) {
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
    AfterqueryObj.log('query args:', out);
    AfterqueryObj.log('query arglist:', outlist);
    return {
      get: function(key) { return out[key]; },
      all: outlist
    };
  };



AfterqueryObj.looksLikeUrl = function(s) {
    var IS_URL_RE = RegExp('^(http|https)://');
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
  };


AfterqueryObj.htmlEscape = function(s) {
    if (s == undefined) {
      return s;
    }
    return s.replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;')
        .replace(/\n/g, '<br>\n');
  };


AfterqueryObj.dataToGvizTable = function(grid, options) {
    if (!options) options = {};
    var is_html = options.allowHtml;
    var headers = grid.headers, data = grid.data, types = grid.types;
    var dheaders = [];
    for (var i in headers) {
      dheaders.push({
        id: headers[i],
        label: headers[i],
        type: (types[i] != AfterqueryObj.T_BOOL || !options.bool_to_num) ? types[i] : AfterqueryObj.T_NUM
      });
    }
    var ddata = [];
    for (var rowi in data) {
      var row = [];
      for (var coli in data[rowi]) {
        var cell = data[rowi][coli];
        if (is_html && types[coli] === AfterqueryObj.T_STRING) {
          cell = cell.toString();
          var urlresult = AfterqueryObj.looksLikeUrl(cell);
          if (urlresult) {
            cell = '<a href="' + encodeURI(urlresult[0]) + '">' +
                AfterqueryObj.htmlEscape(urlresult[1]) + '</a>';
          } else {
            cell = AfterqueryObj.htmlEscape(cell);
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
        if (grid.types[coli] !== AfterqueryObj.T_NUM) continue;
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
        if (grid.types[coli] == AfterqueryObj.T_NUM) {
          var formatter = new google.visualization.ColorFormat();
          var mn, mx;
          if (options.intensify == 'xy') {
            mn = minval;
            mx = maxval;
          } else if (options.intensify == 'y') {
            AfterqueryObj.log(colmin, colmax);
            mn = colmin[coli];
            mx = colmax[coli];
          } else if (options.intensify == 'x') {
            throw new Error('sorry, intensify=x not supported yet');
          } else {
            throw new Error("unknown intensify= mode '" +
                            options.intensify + "'");
          }
          AfterqueryObj.log('coli=' + coli + ' mn=' + mn + ' mx=' + mx);
          formatter.addGradientRange(mn - 1, 0, null, '#f88', '#fff');
          formatter.addGradientRange(0, mx + 1, null, '#fff', '#88f');
          formatter.format(datatable, parseInt(coli));
        }
      }
    }
    return datatable;
  };



AfterqueryObj.T_NUM = 'number';
AfterqueryObj.T_DATE = 'date';
AfterqueryObj.T_DATETIME = 'datetime';
AfterqueryObj.T_BOOL = 'boolean';
AfterqueryObj.T_STRING = 'string';


AfterqueryObj.guessTypes = function(data) {
    var CANT_NUM = 1;
    var CANT_BOOL = 2;
    var CANT_DATE = 4;
    var CANT_DATETIME = 8;
    var impossible = [];
    for (var rowi in data) {
      var row = data[rowi];
      for (var coli in row) {
        impossible[coli] |= 0;
        var cell = row[coli];
        if (cell == '' || cell == null) continue;
        var d = AfterqueryObj.myParseDate(cell);
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
    AfterqueryObj.log('guessTypes impossibility list:', impossible);
    var types = [];
    for (var coli in impossible) {
      var imp = impossible[coli];
      if (!(imp & CANT_BOOL)) {
        types[coli] = AfterqueryObj.T_BOOL;
      } else if (!(imp & CANT_DATE)) {
        types[coli] = AfterqueryObj.T_DATE;
      } else if (!(imp & CANT_DATETIME)) {
        types[coli] = AfterqueryObj.T_DATETIME;
      } else if (!(imp & CANT_NUM)) {
        types[coli] = AfterqueryObj.T_NUM;
      } else {
        types[coli] = AfterqueryObj.T_STRING;
      }
    }
    return types;
  };


AfterqueryObj.myParseDate = function(s) {
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
  };


AfterqueryObj.zpad = function(n, width) {
    var s = '' + n;
    while (s.length < width) s = '0' + s;
    return s;
  };


AfterqueryObj.dateToStr = function(d) {
    if (!d) return '';
    return (d.getFullYear() + '-' +
            AfterqueryObj.zpad(d.getMonth() + 1, 2) + '-' +
            AfterqueryObj.zpad(d.getDate(), 2));
  };


AfterqueryObj.dateTimeToStr = function(d) {
    if (!d) return '';
    var msec = d.getMilliseconds();
    return (AfterqueryObj.dateToStr(d) + ' ' +
            AfterqueryObj.zpad(d.getHours(), 2) + ':' +
            AfterqueryObj.zpad(d.getMinutes(), 2) + ':' +
            AfterqueryObj.zpad(d.getSeconds(), 2) +
            (msec ? ('.' + AfterqueryObj.zpad(msec, 3)) : ''));
  };


AfterqueryObj.prototype.convertTypes = function(data, types) {
    for (var coli in types) {
      var type = types[coli];
      if (type === AfterqueryObj.T_DATE || type === AfterqueryObj.T_DATETIME) {
        for (var rowi in data) {
          data[rowi][coli] = AfterqueryObj.myParseDate(data[rowi][coli]);
        }
      } else if (type === AfterqueryObj.T_NUM || type === AfterqueryObj.T_BOOL) {
        for (var rowi in data) {
          var v = data[rowi][coli];
          if (v != null && v != '') {
            data[rowi][coli] = v * 1;
          }
        }
      }
    }
  };


AfterqueryObj.prototype.colNameToColNum = function(grid, colname) {
    var keycol = (colname == '*') ? 0 : grid.headers.indexOf(colname);
    if (keycol < 0) {
      throw new Error('unknown column name "' + colname + '"');
    }
    return keycol;
  };


AfterqueryObj.prototype.FUNC_RE = /^(\w+)\((.*)\)$/;

AfterqueryObj.prototype.keyToColNum = function(grid, key) {
    var g = this.FUNC_RE.exec(key);
    if (g) {
      return this.colNameToColNum(grid, g[2]);
    } else {
      return this.colNameToColNum(grid, key);
    }
  };


AfterqueryObj.prototype._groupByLoop = function(ingrid, keys, initval, addcols_func, putvalues_func) {
    var outgrid = {headers: [], data: [], types: []};
    var keycols = [];
    for (var keyi in keys) {
      var colnum = this.keyToColNum(ingrid, keys[keyi]);
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
  };

AfterqueryObj.prototype.agg_funcs = {
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
      return AfterqueryObj.prototype.agg_funcs.sum(l) / AfterqueryObj.prototype.agg_funcs.count_nz(l);
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
      var avg = AfterqueryObj.prototype.agg_funcs.avg(l);
      var sumsq = 0.0;
      for (var i in l) {
        var d = parseFloat(l[i]) - avg;
        if (d) sumsq += d * d;
      }
      return Math.sqrt(sumsq);
    },
    color: function(l,aqo) {
      for (var i in l) {
        var v = l[i];
        if (!(v in aqo.colormap)) {
          aqo.colormap[v] = ++aqo.next_color;
        }
        return aqo.colormap[v];
      }
    }
  };
AfterqueryObj.prototype.agg_funcs.count.return_type = AfterqueryObj.T_NUM;
AfterqueryObj.prototype.agg_funcs.count_nz.return_type = AfterqueryObj.T_NUM;
AfterqueryObj.prototype.agg_funcs.count_distinct.return_type = AfterqueryObj.T_NUM;
AfterqueryObj.prototype.agg_funcs.sum.return_type = AfterqueryObj.T_NUM;
AfterqueryObj.prototype.agg_funcs.avg.return_type = AfterqueryObj.T_NUM;
AfterqueryObj.prototype.agg_funcs.stddev.return_type = AfterqueryObj.T_NUM;
AfterqueryObj.prototype.agg_funcs.cat.return_type = AfterqueryObj.T_STRING;
AfterqueryObj.prototype.agg_funcs.color.return_type = AfterqueryObj.T_NUM;


AfterqueryObj.prototype.groupBy = function(ingrid, keys, values) {
    // add one value column for every column listed in values.
    var that = this;
    var valuecols = [];
    var valuefuncs = [];
    var addcols_func = function(outgrid) {
      for (var valuei in values) {
        var g = that.FUNC_RE.exec(values[valuei]);
        var field, func;
        if (g) {
          func = that.agg_funcs[g[1]];
          if (!func) {
            throw new Error('unknown aggregation function "' + g[1] + '"');
          }
          field = g[2];
        } else {
          func = null;
          field = values[valuei];
        }
        var colnum = that.keyToColNum(ingrid, field);
        if (!func) {
          if (ingrid.types[colnum] === AfterqueryObj.T_NUM ||
              ingrid.types[colnum] === AfterqueryObj.T_BOOL) {
            func = that.agg_funcs.sum;
          } else {
            func = that.agg_funcs.count;
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

    var outgrid = this._groupByLoop(ingrid, keys, 0,
                               addcols_func, putvalues_func);

    for (var rowi in outgrid.data) {
      var row = outgrid.data[rowi];
      for (var valuei in values) {
        var outcoli = keys.length + parseInt(valuei);
        var func = valuefuncs[valuei];
        row[outcoli] = func(row[outcoli], this);
      }
    }

    return outgrid;
  };


AfterqueryObj.prototype.pivotBy = function(ingrid, rowkeys, colkeys, valkeys) {
    // We generate a list of value columns based on all the unique combinations
    // of (values in colkeys)*(column names in valkeys)
    var that = this;
    var valuecols = {};
    var colkey_outcols = {};
    var colkey_incols = [];
    for (var coli in colkeys) {
      colkey_incols.push(this.keyToColNum(ingrid, colkeys[coli]));
    }
    var addcols_func = function(outgrid) {
      for (var rowi in ingrid.data) {
        var row = ingrid.data[rowi];
        var colkey = [];
        for (var coli in colkey_incols) {
          var colnum = colkey_incols[coli];
          colkey.push(that.stringifiedCol(row[colnum], ingrid.types[colnum]));
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
      AfterqueryObj.log('pivot colkey_outcols', colkey_outcols);
      AfterqueryObj.log('pivot valuecols:', valuecols);
    };

    // by the time pivotBy is called, we're guaranteed that there's only one
    // row with a given (rowkeys+colkeys) key, so there is only one value
    // for each value cell.  Thus we don't need to worry about count/sum here;
    // we just assign the values directly as we see them.
    var putvalues_func = function(outgrid, rowkey, orow, row) {
      var colkey = [];
      for (var coli in colkey_incols) {
        var colnum = colkey_incols[coli];
        colkey.push(that.stringifiedCol(row[colnum], ingrid.types[colnum]));
      }
      for (var coli in valkeys) {
        var xcolkey = colkey.concat([valkeys[coli]]);
        var outcolnum = colkey_outcols[xcolkey];
        var valuecol = valuecols[xcolkey];
        orow[outcolnum] = row[valuecol];
      }
    };

    return this._groupByLoop(ingrid, rowkeys, undefined,
                        addcols_func, putvalues_func);
  };


AfterqueryObj.prototype.stringifiedCol = function(value, typ) {
    if (typ === AfterqueryObj.T_DATE) {
      return AfterqueryObj.dateToStr(value) || '';
    } else if (typ === AfterqueryObj.T_DATETIME) {
      return AfterqueryObj.dateTimeToStr(value) || '';
    } else {
      return (value + '') || '(none)';
    }
  };


AfterqueryObj.prototype.stringifiedCols = function(row, types) {
    var out = [];
    for (var coli in types) {
      out.push(this.stringifiedCol(row[coli], types[coli]));
    }
    return out;
  };


AfterqueryObj.prototype.treeJoinKeys = function(ingrid, nkeys) {
    var outgrid = {
        headers: ['_tree'].concat(ingrid.headers.slice(nkeys)),
        types: [AfterqueryObj.T_STRING].concat(ingrid.types.slice(nkeys)),
        data: []
    };

    for (var rowi in ingrid.data) {
      var row = ingrid.data[rowi];
      var key = row.slice(0, nkeys);
      var newkey = this.stringifiedCols(row.slice(0, nkeys),
                                   ingrid.types.slice(0, nkeys)).join('|');
      outgrid.data.push([newkey].concat(row.slice(nkeys)));
    }
    return outgrid;
  };


AfterqueryObj.prototype.finishTree = function(ingrid, keys) {
    if (keys.length < 1) {
      keys = ['_tree'];
    }
    var outgrid = {headers: ingrid.headers, data: [], types: ingrid.types};
    var keycols = [];
    for (var keyi in keys) {
      keycols.push(this.keyToColNum(ingrid, keys[keyi]));
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
  };


AfterqueryObj.prototype.invertTree = function(ingrid, key) {
    if (!key) {
      key = '_tree';
    }
    var keycol = this.keyToColNum(ingrid, key);
    var outgrid = {headers: ingrid.headers, data: [], types: ingrid.types};
    for (var rowi in ingrid.data) {
      var row = ingrid.data[rowi];
      var cell = row[keycol];
      var outrow = row.slice();
      outrow[keycol] = cell.split('|').reverse().join('|');
      outgrid.data.push(outrow);
    }
    return outgrid;
  };


AfterqueryObj.prototype.crackTree = function(ingrid, key) {
    if (!key) {
      key = '_tree';
    }
    var keycol = this.keyToColNum(ingrid, key);
    var outgrid = {
      headers:
        [].concat(ingrid.headers.slice(0, keycol),
                  ['_id', '_parent'],
                  ingrid.headers.slice(keycol + 1)),
      data: [],
      types:
        [].concat(ingrid.types.slice(0, keycol),
                  [AfterqueryObj.T_STRING, AfterqueryObj.T_STRING],
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
  };


AfterqueryObj.prototype.splitNoEmpty = function(s, splitter) {
    if (!s) return [];
    return s.split(splitter);
  };


AfterqueryObj.prototype.keysOtherThan = function(grid, keys) {
    var out = [];
    var keynames = [];
    for (var keyi in keys) {
      // this converts func(x) notation to just 'x'
      keynames.push(grid.headers[this.keyToColNum(grid, keys[keyi])]);
    }
    for (var coli in grid.headers) {
      if (keynames.indexOf(grid.headers[coli]) < 0) {
        out.push(grid.headers[coli]);
      }
    }
    return out;
  };


AfterqueryObj.prototype.doGroupBy = function(grid, argval) {
    AfterqueryObj.log('groupBy:', argval);
    var parts = argval.split(';', 2);
    var keys = this.splitNoEmpty(parts[0], ',');
    var values;
    if (parts.length >= 2) {
      // if there's a ';' separator, the names after it are the desired
      // value columns (and that list may be empty).
      var tmpvalues = this.splitNoEmpty(parts[1], ',');
      values = [];
      for (var tmpi in tmpvalues) {
        var tmpval = tmpvalues[tmpi];
        if (tmpval == '*') {
          values = values.concat(this.keysOtherThan(grid, keys.concat(values)));
        } else {
          values.push(tmpval);
        }
      }
    } else {
      // if there is no ';' at all, the default is to just pull in all the
      // remaining non-key columns as values.
      values = this.keysOtherThan(grid, keys);
    }
    AfterqueryObj.log('grouping by', keys, values);
    grid = this.groupBy(grid, keys, values);
    AfterqueryObj.log('grid:', grid);
    return grid;
  };


AfterqueryObj.prototype.doTreeGroupBy = function(grid, argval) {
    AfterqueryObj.log('treeGroupBy:', argval);
    var parts = argval.split(';', 2);
    var keys = this.splitNoEmpty(parts[0], ',');
    var values;
    if (parts.length >= 2) {
      // if there's a ';' separator, the names after it are the desired
      // value columns (and that list may be empty).
      values = this.splitNoEmpty(parts[1], ',');
    } else {
      // if there is no ';' at all, the default is to just pull in all the
      // remaining non-key columns as values.
      values = this.keysOtherThan(grid, keys);
    }
    AfterqueryObj.log('treegrouping by', keys, values);
    grid = this.groupBy(grid, keys, values);
    grid = this.treeJoinKeys(grid, keys.length);
    AfterqueryObj.log('grid:', grid);
    return grid;
  };


AfterqueryObj.prototype.doFinishTree = function(grid, argval) {
    AfterqueryObj.log('finishTree:', argval);
    var keys = this.splitNoEmpty(argval, ',');
    AfterqueryObj.log('finishtree with keys', keys);
    grid = this.finishTree(grid, keys);
    AfterqueryObj.log('grid:', grid);
    return grid;
  };


AfterqueryObj.prototype.doInvertTree = function(grid, argval) {
    AfterqueryObj.log('invertTree:', argval);
    var keys = this.splitNoEmpty(argval, ',');
    AfterqueryObj.log('invertTree with key', keys[0]);
    grid = this.invertTree(grid, keys[0]);
    AfterqueryObj.log('grid:', grid);
    return grid;
  };


AfterqueryObj.prototype.doCrackTree = function(grid, argval) {
    AfterqueryObj.log('crackTree:', argval);
    var keys = this.splitNoEmpty(argval, ',');
    AfterqueryObj.log('cracktree with key', keys[0]);
    grid = this.crackTree(grid, keys[0]);
    AfterqueryObj.log('grid:', grid);
    return grid;
  };


AfterqueryObj.prototype.doPivotBy = function(grid, argval) {
    AfterqueryObj.log('pivotBy:', argval);

    // the parts are rowkeys;colkeys;values
    var parts = argval.split(';', 3);
    var rowkeys = this.splitNoEmpty(parts[0], ',');
    var colkeys = this.splitNoEmpty(parts[1], ',');
    var values;
    if (parts.length >= 3) {
      // if there's a second ';' separator, the names after it are the desired
      // value columns.
      values = this.splitNoEmpty(parts[2], ',');
    } else {
      // if there is no second ';' at all, the default is to just pull
      // in all the remaining non-key columns as values.
      values = this.keysOtherThan(grid, rowkeys.concat(colkeys));
    }

    // first group by the rowkeys+colkeys, so there is only one row for each
    // unique rowkeys+colkeys combination.
    grid = this.groupBy(grid, rowkeys.concat(colkeys), values);
    AfterqueryObj.log('tmpgrid:', grid);

    // now actually do the pivot.
    grid = this.pivotBy(grid, rowkeys, colkeys, values);

    return grid;
  };


AfterqueryObj.prototype.filterBy = function(ingrid, key, op, values) {
    var outgrid = {headers: ingrid.headers, data: [], types: ingrid.types};
    var keycol = this.keyToColNum(ingrid, key);
    var wantvals = [];
    for (var valuei in values) {
      if (ingrid.types[keycol] === AfterqueryObj.T_NUM) {
        wantvals.push(parseFloat(values[valuei]));
      } else if (ingrid.types[keycol] === AfterqueryObj.T_DATE ||
                 ingrid.types[keycol] === AfterqueryObj.T_DATETIME) {
        wantvals.push(AfterqueryObj.dateTimeToStr(AfterqueryObj.myParseDate(values[valuei])));
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
      if (keytype == AfterqueryObj.T_DATE || keytype == AfterqueryObj.T_DATETIME) {
        cell = AfterqueryObj.dateTimeToStr(cell);
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
  };


AfterqueryObj.prototype.trySplitOne = function(argval, splitstr) {
    var pos = argval.indexOf(splitstr);
    if (pos >= 0) {
      return [argval.substr(0, pos).trim(),
              argval.substr(pos + splitstr.length).trim()];
    } else {
      return;
    }
  };


AfterqueryObj.prototype.doFilterBy = function(grid, argval) {
    AfterqueryObj.log('filterBy:', argval);
    var ops = ['>=', '<=', '==', '!=', '<>', '>', '<', '='];
    var parts;
    for (var opi in ops) {
      var op = ops[opi];
      if ((parts = this.trySplitOne(argval, op))) {
        var matches = parts[1].split(',');
        AfterqueryObj.log('filterBy parsed:', parts[0], op, matches);
        grid = this.filterBy(grid, parts[0], op, matches);
        AfterqueryObj.log('grid:', grid);
        return grid;
      }
    }
    throw new Error('unknown filter operation in "' + argval + '"');
    return grid;
  };


AfterqueryObj.prototype.queryBy = function(ingrid, words) {
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
  };


AfterqueryObj.prototype.doQueryBy = function(grid, argval) {
    AfterqueryObj.log('queryBy:', argval);
    grid = this.queryBy(grid, argval.split(','));
    AfterqueryObj.log('grid:', grid);
    return grid;
  };


AfterqueryObj.prototype.deltaBy = function(ingrid, keys) {
    var outgrid = {headers: ingrid.headers, data: [], types: ingrid.types};
    for (var rowi = 0; rowi < ingrid.data.length; rowi++) {
      var row = ingrid.data[rowi];
      outgrid.data.push(row);
    }

    var keycols = [];
    for (var keyi in keys) {
      var key = keys[keyi];
      keycols.push(this.keyToColNum(ingrid, key));
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
        } else if (outgrid.types[keycol] === AfterqueryObj.T_NUM) {
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
  };


AfterqueryObj.prototype.doDeltaBy = function(grid, argval) {
    AfterqueryObj.log('deltaBy:', argval);
    grid = this.deltaBy(grid, argval.split(','));
    AfterqueryObj.log('grid:', grid);
    return grid;
  };


AfterqueryObj.prototype.unselectBy = function(ingrid, keys) {
    var outgrid = {headers: [], data: [], types: []};
    var keycols = {};
    for (var keyi in keys) {
      var key = keys[keyi];
      var col = this.keyToColNum(ingrid, key);
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
  };


AfterqueryObj.prototype.doUnselectBy = function(grid, argval) {
    AfterqueryObj.log('unselectBy:', argval);
    grid = this.unselectBy(grid, argval.split(','));
    AfterqueryObj.log('grid:', grid);
    return grid;
  };


AfterqueryObj.prototype.orderBy = function(grid, keys) {
    var that = this;
    var keycols = [];
    for (var keyi in keys) {
      var key = keys[keyi];
      var invert = 1;
      if (key[0] == '-') {
        invert = -1;
        key = key.substr(1);
      }
      keycols.push([this.keyToColNum(grid, key), invert]);
    }
    AfterqueryObj.log('sort keycols', keycols);
    var comparator = function(a, b) {
      for (var keyi in keycols) {
        var keycol = keycols[keyi][0], invert = keycols[keyi][1];
        var av = a[keycol], bv = b[keycol];
        if (grid.types[keycol] === AfterqueryObj.T_NUM) {
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
  };


AfterqueryObj.prototype.doOrderBy = function(grid, argval) {
    AfterqueryObj.log('orderBy:', argval);
    grid = this.orderBy(grid, argval.split(','));
    AfterqueryObj.log('grid:', grid);
    return grid;
  };


AfterqueryObj.prototype.extractRegexp = function(grid, colname, regexp) {
    var r = RegExp(regexp);
    var colnum = this.keyToColNum(grid, colname);
    var typ = grid.types[colnum];
    grid.types[colnum] = AfterqueryObj.T_STRING;
    for (var rowi in grid.data) {
      var row = grid.data[rowi];
      var match = r.exec(this.stringifiedCol(row[colnum], typ));
      if (match) {
        row[colnum] = match.slice(1).join('');
      } else {
        row[colnum] = '';
      }
    }
    return grid;
  };


AfterqueryObj.prototype.doExtractRegexp = function(grid, argval) {
    AfterqueryObj.log('extractRegexp:', argval);
    var parts = this.trySplitOne(argval, '=');
    var colname = parts[0], regexp = parts[1];
    if (regexp.indexOf('(') < 0) {
      throw new Error('extract_regexp should have at least one (regex group)');
    }
    grid = this.extractRegexp(grid, colname, regexp);
    AfterqueryObj.log('grid:', grid);
    return grid;
  };


AfterqueryObj.prototype.quantize = function(grid, colname, quants) {
    var colnum = this.keyToColNum(grid, colname);
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
  };


AfterqueryObj.prototype.doQuantize = function(grid, argval) {
    AfterqueryObj.log('quantize:', argval);
    var parts = this.trySplitOne(argval, '=');
    var colname = parts[0], quants = parts[1].split(',');
    grid = this.quantize(grid, colname, quants);
    AfterqueryObj.log('grid:', grid);
    return grid;
  };

AfterqueryObj.prototype.doRename = function(ingrid, argval) {
    AfterqueryObj.log('rename:', argval);
    var parts = this.trySplitOne(argval, '=');
    var src = parts[0];
    var dst = parts[1];
    var grid = {
      data: ingrid.data,
      types: ingrid.types,
      headers: [],
    };
    for(var i = 0; i < ingrid.headers.length; i++) {
      var header = ingrid.headers[i];
      if(header === src) { header = dst; }
      grid.headers.push(header);
    }
    AfterqueryObj.log('grid:', grid);
    return grid;
  };


AfterqueryObj.prototype.yspread = function(grid) {
    for (var rowi in grid.data) {
      var row = grid.data[rowi];
      var total = 0;
      for (var coli in row) {
        if (grid.types[coli] == AfterqueryObj.T_NUM && row[coli]) {
          total += Math.abs(row[coli] * 1);
        }
      }
      if (!total) total = 1;
      for (var coli in row) {
        if (grid.types[coli] == AfterqueryObj.T_NUM && row[coli]) {
          row[coli] = row[coli] * 1 / total;
        }
      }
    }
    return grid;
  };


AfterqueryObj.prototype.doYSpread = function(grid, argval) {
    AfterqueryObj.log('yspread:', argval);
    if (argval) {
      throw new Error('yspread: no argument expected');
    }
    grid = this.yspread(grid);
    AfterqueryObj.log('grid:', grid);
    return grid;
  };


AfterqueryObj.prototype.doLimit = function(ingrid, limit) {
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
  };


AfterqueryObj.prototype.limitDecimalPrecision = function(grid) {
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
  };


AfterqueryObj.prototype.fillNullsWithZero = function(grid) {
    for (var rowi in grid.data) {
      var row = grid.data[rowi];
      for (var coli in row) {
        if (grid.types[coli] === AfterqueryObj.T_NUM && row[coli] == undefined) {
          row[coli] = 0;
        }
        if (grid.types[coli] === AfterqueryObj.T_STRING && row[coli] == undefined) {
          row[coli] = "_undefined_";
        }
      }
    }
    return grid;
  };


AfterqueryObj.prototype.isString = function(v) {
    return v.charAt !== undefined;
  };


AfterqueryObj.prototype.isArray = function(v) {
    return v.splice !== undefined;
  };


AfterqueryObj.prototype.isObject = function(v) {
    return typeof(v) === 'object';
  };


AfterqueryObj.prototype.isDate = function(v) {
    return v.getDate !== undefined;
  };


AfterqueryObj.prototype.isScalar = function(v) {
    return v == undefined || this.isString(v) || !this.isObject(v) || this.isDate(v);
  };


AfterqueryObj.prototype.check2d = function(rawdata) {
    if (!this.isArray(rawdata)) return false;
    for (var rowi = 0; rowi < rawdata.length && rowi < 5; rowi++) {
      var row = rawdata[rowi];
      if (!this.isArray(row)) return false;
      for (var coli = 0; coli < row.length; coli++) {
        var col = row[coli];
        if (!this.isScalar(col)) return false;
      }
    }
    return true;
  };


AfterqueryObj.prototype._copyObj = function(out, v) {
    for (var key in v) {
      out[key] = v[key];
    }
    return out;
  };


AfterqueryObj.prototype.copyObj = function(v) {
    return this._copyObj({}, v);
  };


AfterqueryObj.prototype.multiplyLists = function(out, l1, l2) {
    if (l1 === undefined) throw new Error('l1 undefined');
    if (l2 === undefined) throw new Error('l2 undefined');
    for (var l1i in l1) {
      var r1 = l1[l1i];
      for (var l2i in l2) {
        var r2 = l2[l2i];
        var o = this.copyObj(r1);
        this._copyObj(o, r2);
        out.push(o);
      }
    }
    return out;
  };


AfterqueryObj.prototype.flattenDict = function(headers, coldict, rowtmp, d) {
    var out = [];
    var lists = [];
    for (var key in d) {
      var value = d[key];
      if (this.isScalar(value)) {
        if (coldict[key] === undefined) {
          coldict[key] = headers.length;
          headers.push(key);
        }
        rowtmp[key] = value;
      } else if (this.isArray(value)) {
        lists.push(this.flattenList(headers, coldict, value));
      } else {
        lists.push(this.flattenDict(headers, coldict, rowtmp, value));
      }
    }

    // now multiply all the lists together
    var tmp1 = [{}];
    while (lists.length) {
      var tmp2 = [];
      this.multiplyLists(tmp2, tmp1, lists.shift());
      tmp1 = tmp2;
    }

    // this is apparently the "right" way to append a list to a list.
    Array.prototype.push.apply(out, tmp1);
    return out;
  };


AfterqueryObj.prototype.flattenList = function(headers, coldict, rows) {
    var out = [];
    for (var rowi in rows) {
      var row = rows[rowi];
      var rowtmp = {};
      var sublist = this.flattenDict(headers, coldict, rowtmp, row);
      this.multiplyLists(out, [rowtmp], sublist);
    }
    return out;
  };


AfterqueryObj.prototype.gridFromData = function(rawdata) {
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
    } else if (this.check2d(rawdata)) {
      // simple [[cols...]...] (two-dimensional array) format, where
      // the first row is the headers.
      headers = rawdata[0];
      data = rawdata.slice(1);
    } else if (this.isArray(rawdata)) {
      // assume datacube format, which is a nested set of lists and dicts.
      // A dict contains a list of key (column name) and value (cell content)
      // pairs.  A list contains a set of lists or dicts, which corresponds
      // to another "dimension" of the cube.  To flatten it, we have to
      // replicate the row once for each element in the list.
      var coldict = {};
      headers = [];
      var rowdicts = this.flattenList(headers, coldict, rawdata);
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
    types = AfterqueryObj.guessTypes(data);
    this.convertTypes(data, types);
    return {headers: headers, data: data, types: types};
  };


AfterqueryObj.prototype.enqueue = function(queue, stepname, func) {
    queue.push([stepname, func]);
  };


AfterqueryObj.prototype.runqueue = function(queue, ingrid, done, showstatus, wrap_each, after_each) {
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


AfterqueryObj.prototype.maybeSet = function(dict, key, value) {
    if (!(key in dict)) {
      dict[key] = value;
    }
  };


AfterqueryObj.prototype.addTransforms = function(queue, args) {
    var that = this;
    var trace = args.get('trace');
    var argi;

    // helper function for synchronous transformations (ie. ones that return
    // the output grid rather than calling a callback)
    var transform = function(f, arg) {
      that.enqueue(queue, args.all[argi][0] + '=' + args.all[argi][1],
              function(ingrid, done) {
        var outgrid = f(ingrid, arg);
        done(outgrid);
      });
    };

    for (var argi in args.all) {
      var argkey = args.all[argi][0], argval = args.all[argi][1];
      if (argkey == 'group') {
        transform(function(g,a){return that.doGroupBy(g,a);}, argval);
      } else if (argkey == 'treegroup') {
        transform(function(g,a){return that.doTreeGroupBy(g,a);}, argval);
      } else if (argkey == 'finishtree') {
        transform(function(g,a){return that.doFinishTree(g,a);}, argval);
      } else if (argkey == 'inverttree') {
        transform(function(g,a){return that.doInvertTree(g,a);}, argval);
      } else if (argkey == 'cracktree') {
        transform(function(g,a){return that.doCrackTree(g,a);}, argval);
      } else if (argkey == 'pivot') {
        transform(function(g,a){return that.doPivotBy(g,a);}, argval);
      } else if (argkey == 'filter') {
        transform(function(g,a){return that.doFilterBy(g,a);}, argval);
      } else if (argkey == 'q') {
        transform(function(g,a){return that.doQueryBy(g,a);}, argval);
      } else if (argkey == 'limit') {
        transform(function(g,a){return that.doLimit(g,a);}, argval);
      } else if (argkey == 'delta') {
        transform(function(g,a){return that.doDeltaBy(g,a);}, argval);
      } else if (argkey == 'unselect') {
        transform(function(g,a){return that.doUnselectBy(g,a);}, argval);
      } else if (argkey == 'order') {
        transform(function(g,a){return that.doOrderBy(g,a);}, argval);
      } else if (argkey == 'extract_regexp') {
        transform(function(g,a){return that.doExtractRegexp(g,a);}, argval);
      } else if (argkey == 'quantize') {
        transform(function(g,a){return that.doQuantize(g,a);}, argval);
      } else if (argkey == 'rename') {
        transform(function(g,a){return that.doRename(g,a);}, argval);
      } else if (argkey == 'yspread') {
        transform(function(g,a){return that.doYSpread(g,a);}, argval);
      }
    }
  };


AfterqueryObj.prototype.dyIndexFromX = function(dychart, x) {
    // TODO(apenwarr): consider a binary search
    for (var i = dychart.numRows() - 1; i >= 0; i--) {
      if (dychart.getValue(i, 0) <= x) {
        break;
      }
    }
    return i;
  };


AfterqueryObj.prototype.createTracesChart = function(grid, el, colsPerChart) {
    var that = this;
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
            var otheridx = that.dyIndexFromX(charts[charti], data[idx][0]);
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
  };


AfterqueryObj.NaNToZeroFormatter = function(dt, col) {
  for(var row = 0; row<dt.getNumberOfRows(); row++) {
    if(isNaN(dt.getValue(row, col))) {
      dt.setValue(row, col, 0);
      dt.setFormattedValue(row, col, '');
    }
  }
}

AfterqueryObj.prototype.addRenderers = function(queue, args, more_options_in) {
    var that = this;
    var has_more_opts = more_options_in !== undefined;
    var more_options = more_options_in;
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
    var el = $(this.elid("vizchart"))[0];

    this.enqueue(queue, 'gentable', function(grid, done) {
      // Some charts react badly to missing values, so fill them in.
      grid = that.fillNullsWithZero(grid);
      if (chartops) {
        var chartbits = chartops.split(',');
        var charttype = chartbits.shift();
        for (var charti in chartbits) {
          var kv = that.trySplitOne(chartbits[charti], '=');
          options[kv[0]] = kv[1];
        }
        grid = that.limitDecimalPrecision(grid);

        // Scan and add all args not for afterquery to GViz options.
        that.scanGVizChartOptions(args, options);

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
            grid = that.finishTree(grid, ['_tree']);
            grid = that.crackTree(grid, '_tree');
          }
          that.maybeSet(options, 'maxDepth', 3);
          that.maybeSet(options, 'maxPostDepth', 1);
          that.maybeSet(options, 'showScale', 1);
          t = new google.visualization.TreeMap(el);
        } else if (charttype == 'candle' || charttype == 'candlestick') {
          t = new google.visualization.CandlestickChart(el);
        } else if (charttype == 'timeline') {
          t = new google.visualization.AnnotatedTimeLine(el);
        } else if (charttype == 'dygraph' || charttype == 'dygraph+errors') {
          t = new Dygraph.GVizChart(el);
          that.maybeSet(options, 'showRoller', true);
          if (charttype == 'dygraph+errors') {
            options.errorBars = true;
          }
        } else if (charttype == 'traces' || charttype == 'traces+minmax') {
          t = that.createTracesChart(grid, el,
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
        datatable = AfterqueryObj.dataToGvizTable(grid, gridoptions);

        var dateformat = new google.visualization.DateFormat({
          pattern: 'yyyy-MM-dd'
        });
        var datetimeformat = new google.visualization.DateFormat({
          pattern: 'yyyy-MM-dd HH:mm:ss'
        });
        for (var coli = 0; coli < grid.types.length; coli++) {
          if (grid.types[coli] === AfterqueryObj.T_DATE) {
            dateformat.format(datatable, coli);
          } else if (grid.types[coli] === AfterqueryObj.T_DATETIME) {
            datetimeformat.format(datatable, coli);
          } else if (grid.types[coli] === AfterqueryObj.T_NUM) {
            if(has_more_opts &&
              more_options.num_pattern !== undefined) {
              var numformat = new google.visualization.NumberFormat({
                pattern: more_options.num_pattern
              });
              numformat.format(datatable, coli);
              AfterqueryObj.NaNToZeroFormatter(datatable, coli);
            }
          }
        }
      }
      done(grid);
    });

    this.enqueue(queue, chartops ? 'chart=' + chartops : 'view',
            function(grid, done) {
      if (grid.data.length) {
        var doRender = function() {
          var wantwidth = trace ? $(el).innerWidth - 40 : $(el).innerWidth;
          $(el).width(wantwidth);
          if(!has_more_opts || !more_options.disable_height) {
            $(el).height(window.innerHeight);
            options.height = window.innerHeight;
          }
          t.draw(datatable, options);
          if(has_more_opts && more_options.select_handler !== undefined) {
            google.visualization.events.addListener(t, 'select', function(e) { more_options.select_handler(e,t, datatable); });
          }
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
  };

AfterqueryObj.prototype.scanGVizChartOptions = function(args, options) {
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
      this.addGVizChartOption(options, key, allArgs[i][1]);
    }
    AfterqueryObj.log('Options sent to GViz');
    AfterqueryObj.log(options);
  };

AfterqueryObj.prototype.addGVizChartOption = function(options, key, value) {
    if (key.indexOf('.') > -1) {
      var subObjects = key.split('.');
      if (!options[subObjects[0]]) {
        options[subObjects[0]] = {};
      }
      this.addGVizChartOption(options[subObjects[0]],
        key.substring(key.indexOf('.') + 1), value);
    } else {
      options[key] = value;
    }
  };

// Needs to be shared by all instances.
AfterqueryObj.vizstep = 0;

AfterqueryObj.prototype.finishQueue = function(queue, args, done) {
    var that = this;
    var trace = args.get('trace');
    if (trace) {
      var prevdata;
      var after_each = function(grid, stepi, nsteps, text, msec_time) {
        AfterqueryObj.vizstep++;
        $(that.elid('vizlog')).append('<div class="vizstep" id="step' + AfterqueryObj.vizstep + '">' +
                            '  <div class="text"></div>' +
                            '  <div class="grid"></div>' +
                            '</div>');
        $('#step' + AfterqueryObj.vizstep + ' .text').text('Step ' + stepi +
                                           ' (' + msec_time + 'ms):  ' +
                                           text);
        var viewel = $('#step' + AfterqueryObj.vizstep + ' .grid');
        if (prevdata != grid.data) {
          var t = new google.visualization.Table(viewel[0]);
          var datatable = AfterqueryObj.dataToGvizTable({
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
      this.runqueue(queue, null, done, function(s,s2){that.showstatus(s,s2);}, function(f){return that.wrap(f);}, after_each);
    } else {
      this.runqueue(queue, null, done, function(s,s2){that.showstatus(s,s2);}, function(f){return that.wrap(f);});
    }
  };


AfterqueryObj.prototype.gotError = function(url, jqxhr, status) {
    this.showstatus('');
    $(this.elid('vizraw')).html('<a href="' + encodeURI(url) + '">' +
                      encodeURI(url) +
                      '</a>');
    throw new Error('error getting url "' + url + '": ' +
                    status + ': ' +
                    'visit the data page and ensure it\'s valid jsonp.');
  };


AfterqueryObj.prototype.argsToArray = function(args) {
    // call Array's slice() function on an 'arguments' structure, which is
    // like an array but missing functions like slice().  The result is a
    // real Array object, which is more useful.
    return [].slice.apply(args);
  };


AfterqueryObj.prototype.wrap = function(func) {
    // pre_args is the arguments as passed at wrap() time
    var that = this;
    var pre_args = this.argsToArray(arguments).slice(1);
    var f = function() {
      try {
        // post_args is the arguments as passed when calling f()
        var post_args = that.argsToArray(arguments);
        return func.apply(null, pre_args.concat(post_args));
      } catch (e) {
        $(that.elid('vizchart')).hide();
        $(that.elid('vizstatus')).css('position', 'relative');
        if(that.rootid) {
          $("#" + that.rootid + ' .vizstep').show();
        } else {
          $('.vizstep').show();
        }
        that.err(e);
        that.err("<p><a href='/help'>here's the documentation</a>");
        throw e;
      }
    };
    return f;
  };


AfterqueryObj.prototype.urlMinusPath = function(url) {
    var URL_RE = RegExp('^((\\w+:)?(//[^/]*)?)');
    var g = URL_RE.exec(url);
    if (g && g[1]) {
      return g[1];
    } else {
      return url;
    }
  };


AfterqueryObj.prototype.checkUrlSafety = function(url) {
    if (/[<>"''"]/.exec(url)) {
      throw new Error('unsafe url detected. encoded=' + encodedURI(url));
    }
  };


AfterqueryObj.prototype.extendDataUrl = function(url) {
    // some services expect callback=, some expect jsonp=, so supply both
    var plus = 'callback=jsonp&jsonp=jsonp';
    var hostpart = this.urlMinusPath(url);
    var auth;
    if( (/\/\//.exec(url))) {
      // No "//"? Probably a local file, and this operation
      // is doomed.
      auth = this.localStorage[['auth', hostpart]];
    }
    if (auth) {
      plus += '&auth=' + encodeURIComponent(auth);
    }

    if (url.indexOf('?') >= 0) {
      return url + '&' + plus;
    } else {
      return url + '?' + plus;
    }
  };


AfterqueryObj.prototype.extractJsonFromJsonp = function(text, success_func) {
    var data = text.trim();
    var start = data.indexOf('jsonp(');
    if (start >= 0) {
      data = data.substr(start + 6, data.length - start - 6 - 2);
      data = data.trim();
    }

    // Drop spurious trailing comma.
    // Likely in programmatically generated data where a comma is
    // appended to every record.
    if(data[data.length-1] === ',') {
      data = data.slice(0,-1);
    }

    // Ensure there is a "[" "]" wrapper around the whole thing.
    // Likely in programmatically generated data where new data is
    // regularly appended.  Maintaining the framing "[" "]" is a 
    // nuisance, so it doesn't get done.
    if(data.charAt(0) !== '[') {
      data = "[" + data + "]";
    }

    data = JSON.parse(data);
    success_func(data);
  };


AfterqueryObj.prototype.getUrlData_xhr = function(url, success_func, error_func) {
    var that = this;
    jQuery.support.cors = true;
    jQuery.ajax(url, {
      headers: { 'X-DataSource-Auth': 'a' },
      xhrFields: { withCredentials: true },
      dataType: 'text',
      success: function(text) { that.extractJsonFromJsonp(text, success_func); },
      error: error_func
    });
  };


AfterqueryObj.prototype.getUrlData_jsonp = function(url, success_func, error_func) {
    var that = this;
    var iframe = document.createElement('iframe');
    iframe.style.display = 'none';

    iframe.onload = function() {
      var successfunc_called;
      var real_success_func = function(data) {
        AfterqueryObj.log('calling success_func');
        success_func(data);
        successfunc_called = true;
      };

      // the default jsonp callback
      iframe.contentWindow.jsonp = real_success_func;

      // a callback the jsonp can execute if oauth2 authentication is needed
      iframe.contentWindow.tryOAuth2 = function(oauth2_url) {
        var hostpart = that.urlMinusPath(oauth2_url);
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
          AfterqueryObj.log('oauth2 redirect:', want_url);
          that.checkUrlSafety(want_url);
          document.write('Click here to ' +
                         '<a target="_top" ' +
                         '  href="' + want_url +
                         '">authorize the data source</a>.');
        } else {
          AfterqueryObj.log('no oauth2 service known for host', hostpart);
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
        that.err(message + ' url=' + xurl + ' line=' + lineno);
      };

      iframe.contentWindow.onpostscript = function() {
        if (successfunc_called) {
          AfterqueryObj.log('json load was successful.');
        } else {
          that.err('Error loading data; check javascript console for details.');
          that.err('<a href="' + encodeURI(url) + '">' + encodeURI(url) + '</a>');
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
  };


AfterqueryObj.prototype.getUrlData = function(url, success_func, error_func) {
    var that = this;
    AfterqueryObj.log('fetching data url:', url);
    var onError = function(xhr, msg) {
      AfterqueryObj.log('xhr returned error:', msg);
      AfterqueryObj.log('(trying jsonp instead)');
      this.getUrlData_jsonp(url, success_func, error_func);
    };
    this.getUrlData_xhr(url, success_func, onError);
  };


AfterqueryObj.prototype.addUrlGettersDirect = function(queue, url, startdata) {
    var that = this;
    if (!startdata) {
      AfterqueryObj.log('original data url:', url);
      if (!url) throw new Error('Missing url= in query parameter');
      if (url.indexOf('//') == 0) url = window.location.protocol + url;
      url = this.extendDataUrl(url);
      this.showstatus('Loading <a href="' + encodeURI(url) + '">data</a>...');

      this.enqueue(queue, 'get data', function(_, done) {
        that.getUrlData(url, that.wrap(done), that.wrap(function(u,j,d){that.gotError(u,j,s);}, url));
      });
    } else {
      this.enqueue(queue, 'init data', function(_, done) {
        done(startdata);
      });
    }

    this.enqueue(queue, 'parse', function(rawdata, done) {
      AfterqueryObj.log('rawdata:', rawdata);
      var outgrid = that.gridFromData(rawdata);
      AfterqueryObj.log('grid:', outgrid);
      done(outgrid);
    });
  };

AfterqueryObj.prototype.addUrlGetters = function(queue, args, startdata) {
  this.addUrlGettersDirect(queue, args.get('url'), startdata);
}


AfterqueryObj.prototype.addKeepData = function(queue, args) {
  var box = {};
  this.enqueue(queue, 'keep', function(grid, done) {
    box.value = grid;
    done(grid);
    }
  );
  return box;
}

AfterqueryObj.prototype.exec = function(query, startdata, done) {
    var args = AfterqueryObj.parseArgs(query);
    var queue = [];
    this.addUrlGetters(queue, args, startdata);
    this.addTransforms(queue, args);
    this.runqueue(queue, startdata, done);
  };


AfterqueryObj.prototype.render = function(query, startdata, done, more_options) {
    var args = AfterqueryObj.parseArgs(query);
    var editlink = args.get('editlink');
    if (editlink == 0) {
      $(thid.elid('editmenu')).hide();
    }

    var queue = [];
    this.addUrlGetters(queue, args, startdata);
    this.addTransforms(queue, args);
    this.addRenderers(queue, args, more_options);
    this.finishQueue(queue, args, done);
  };


/* Runs get getter (url=X), then returns it. Useful for
   caching. */
AfterqueryObj.prototype.load = function(query, startdata, done) {
  var args = AfterqueryObj.parseArgs(query);
  var queue = [];
  this.addUrlGetters(queue, args, startdata);
  var results = this.addKeepData(queue, args);
  this.finishQueue(queue, args, done);
  return results;
}

/* Runs all of the data transforms, then returns it. Useful
   for creating a version to hand to some other system. */
AfterqueryObj.prototype.load_post_transform = function(query, startdata, done) {
  var args = AfterqueryObj.parseArgs(query);
  var queue = [];
  this.addUrlGetters(queue, args, startdata);
  this.addTransforms(queue, args);
  var results = this.addKeepData(queue, args);
  this.finishQueue(queue, args, done);
  return results;
}


// v8shell compatibility
try {
  var c = window.console;
  AfterqueryObj.console_debug = c.debug;
}
catch (ReferenceError) {
  AfterqueryObj.console.debug = print;
}
// Konqueror compatibility.
if(!AfterqueryObj.console_debug) {
  AfterqueryObj.console.debug = function(){};
}

// v8shell compatibility
try {
  AfterqueryObj.prototype.localStorage = window.localStorage;
} catch (ReferenceError) {
  AfterqueryObj.prototype.localStorage = {};
}


// Original Afterquery interface.
var afterquery = {};
afterquery.render = function(query, startdata, done) {
  var aq = new AfterqueryObj();
  aq.render(query, startdata, done);
}


