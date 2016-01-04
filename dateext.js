////////////////////////////////////////////////////////////////////////////////
// getUTCWeek and getUTCWeekYear are from:
// van den Broek, Taco. Calculate ISO 8601 week and year in javascript. Procuios. 2009-04-22. URL:http://techblog.procurios.nl/k/news/view/33796/14863/calculate-iso-8601-week-and-year-in-javascript.html. Accessed: 2015-12-07. (Archived by WebCite® at http://www.webcitation.org/6dbdvnbtj)
// Released and used under the MIT license.

// Get ISO 8601 week number for a date.
Date.prototype.getUTCWeek = function () {
	"use strict";
	var target  = new Date(this.valueOf());
	var dayNr   = (this.getUTCDay() + 6) % 7;
	target.setUTCDate(target.getUTCDate() - dayNr + 3);
	var firstThursday = target.valueOf();
	target.setUTCMonth(0, 1);
	if (target.getUTCDay() != 4) {
		target.setUTCMonth(0, 1 + ((4 - target.getUTCDay()) + 7) % 7);
	}
	return 1 + Math.ceil((firstThursday - target) / 604800000);
}

// Get ISO 8601 year, assuming we're using week numbers.
Date.prototype.getUTCWeekYear = function () {
	"use strict";
	var target	= new Date(this.valueOf());
	target.setUTCDate(target.getUTCDate() - ((this.getUTCDay() + 6) % 7) + 3);
	return target.getUTCFullYear();
}

// Get ISO 8610 year and week in form "2016-W01"
Date.prototype.getUTCISOWeekDate = function ()  {
	"use strict";
	var week = this.getUTCWeek();
	week = ("0"+week).slice(-2); // Zero pad.
	var year = this.getUTCWeekYear();
	return year+"-W"+week;
}

Date.prototype.getUTCISOYearMonth = function() {
	"use strict";
	var year = this.getUTCFullYear();
	var month = this.getUTCMonth()+1;
	month = ("0"+month).slice(-2); // Zero pad.
	return year+"-"+month;
}

Date.prototype.getUTCISOYearMonthDay = function() {
	"use strict";
	var yearmonth = this.getUTCISOYearMonth();
	var day = this.getUTCDate();
	day = ("0"+day).slice(-2); // Zero pad.
	return yearmonth+"-"+day;
}

// Identical to "new Date(str)", but accepts ISO 8601 week dates (eg "2015-W04"). Dates are assumed to always be in UTC.
// Week dates will return midnight on the Monday morning of that week. 
Date.parseMore = function(str) {
	"use strict";
	var fields = /^(\d\d\d\d)-W(\d\d)$/.exec(str);
	if((!fields) || fields.length !== 3) { return new Date(Date.parse(str)); }
	var year = Number(fields[1]);
	var week = Number(fields[2]);

	// Based on http://stackoverflow.com/a/19375264/16672
	var d = new Date(Date.UTC(year, 0, 1));
	var week_in_ms = 1000*60*60*24*7;
	d.setUTCDate(d.getUTCDate() + 4 - (d.getUTCDay() || 7));
	d.setTime(d.getTime() + week_in_ms * (week + (year == d.getUTCFullYear() ? -1 : 0 )));
	d.setUTCDate(d.getUTCDate() - 3);
	return d;
}

Date.parseDateTime = function(date, time) {
	var RE_DATE = /(\d\d\d\d)[-\/](\d+)[-\/](\d+)/;

	var hits = RE_DATE.exec(date);
	if(!hits) { return; }
	var yyyy = parseInt(hits[1]);
	var mm = parseInt(hits[2]);
	var dd = parseInt(hits[3]);

	if(!time) { time = "00:00"; }
	var RE_TIME = /(\d+):(\d\d)(?::(\d\d)(?:\.(\d\d\d))?)?\s*([AP][M])?/i;
	hits = RE_TIME.exec(time);
	if(!hits) { return; }
	var hour = parseInt(hits[1]);
	var min = parseInt(hits[2]);
	var sec = parseInt(hits[3])||0;
	var millisec = parseInt(hits[4])||0;
	var ampm = hits[5];

	console.log("hour", hour, ampm);
	if(ampm) {
		if(ampm.match(/PM/i) && hour < 12) {
			hour += 12;
		} else if(ampm.match(/AM/i) && hour == 12) {
			hour -= 12;
		}
	}
	console.log("hour", hour, ampm);

	var ret_date = new Date(yyyy, mm-1, dd, hour, min, sec, millisec);
	if(isNaN(ret_date.getTime())) { return; }
	return ret_date;
}
