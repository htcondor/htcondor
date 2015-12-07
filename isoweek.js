////////////////////////////////////////////////////////////////////////////////
// getWeek and getWeekYear are from:
// van den Broek, Taco. Calculate ISO 8601 week and year in javascript. Procuios. 2009-04-22. URL:http://techblog.procurios.nl/k/news/view/33796/14863/calculate-iso-8601-week-and-year-in-javascript.html. Accessed: 2015-12-07. (Archived by WebCite® at http://www.webcitation.org/6dbdvnbtj)
// Released and used under the MIT license.

// Get ISO 8601 week number for a date.
Date.prototype.getWeek = function () {
	var target  = new Date(this.valueOf());
	var dayNr   = (this.getDay() + 6) % 7;
	target.setDate(target.getDate() - dayNr + 3);
	var firstThursday = target.valueOf();
	target.setMonth(0, 1);
	if (target.getDay() != 4) {
		target.setMonth(0, 1 + ((4 - target.getDay()) + 7) % 7);
	}
	return 1 + Math.ceil((firstThursday - target) / 604800000);
}

// Get ISO 8601 year, assuming we're using week numbers.
Date.prototype.getWeekYear = function () {
	var target	= new Date(this.valueOf());
	target.setDate(target.getDate() - ((this.getDay() + 6) % 7) + 3);
	return target.getFullYear();
}

// Get ISO 8610 year and week in form "2016-W01"
Date.prototype.getISOWeekDate = function ()  {
	var week = this.getWeek();
	week = ("0"+week).slice(-2); // Zero pad.
	var year = this.getWeekYear();
	return year+"-W"+week;
}
