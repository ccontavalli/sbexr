'use strict';

function secondsToHms(d) {
  d = Number(d);
  var h = Math.floor(d / 3600);
  var m = Math.floor(d % 3600 / 60);
  var s = Math.floor(d % 3600 % 60);

  var H = h > 0 ? `${h}H `: "";
  var M = m > 0 ? `${m}M `: "";
  var S = s > 0 ? `${s}S` : "";
  return H + M + S;
}

function ShowLabels(selector, data) {
  var labels = {};
  var restricted = {};

  for (const point of data) {
    for (const label of point.label) {
      elements = label.split(":", 1);
      if (!labels[elements[0]]) {
        labels[elements[0]] = {}
      }
      labels[elements[0]][label] = true;
    }
  }
  if (labels.size <= 0) return;

  var categories = Object.keys(labels);
  categories.sort();
  
  for (const category of categories) {
    var toggled = {};
    var id = `${selector}-${category}-labels`;
    var buttons = $(`#${selector}`).append(
      `<br />${category}: <div id="${id}" class="btn-group btn-group-sm text-center split-by-controls">`);
    var elements = Object.keys(labels[category]);
    elements.sort();
    for (const label of elements) {
      var hlabel = label.slice(category.length + 1);
      $(`#${id}`).append(`<button type="button" class="btn btn-default" data-toggle="button" data-label="${label}">${hlabel}</button>`);
    }

    $(`#${id} button`).click(function() {
      var label = $(this).data('label');
      if (toggled[label]) {
        toggled[label] = false;
        delete restricted[label];
      } else {
        toggled[label] = true;
	restricted[label] = true;
      }

      var filtered = data;
      if (Object.keys(restricted).length > 0) {
        filtered = data.filter(function (element) {
          if (!element.label) return false;

          for (const label of element.label) {
            if (restricted[label]) return true;
          }
          return false;
        });
      }
      ShowGraph(selector, filtered);
    });
  }
}

function PrepareData(selector, data) {
  // %Q -> milliseconds since epoch, %s -> seconds since epoch.
  // https://github.com/d3/d3-time-format/blob/master/README.md#timeFormat
  var converter = d3.timeParse('%Q');
  if (selector.endsWith("-rss")) {
    data = data.map(function(t) { t.time = converter(t.time); t.value = t.value * 1024; return t; });
  }  else if (selector.endsWith("-time")) {
    data = data.map(function(t) { t.time = converter(t.time); t.value = t.value / 1000; return t; });
  } else {
    converter = d3.timeParse('%s');
    data = data.map(function(t) { t.time = converter(t.time); return t; });
  }
  return data;
}

function ShowGraph(selector, data) {
  var formatter = d3.format(',.2s');
  if (selector.endsWith("-time")) {
    formatter = secondsToHms
  }

  MG.data_graphic({
      title: selector,
      description: "",
      data: data,
      width: 1200,
      height: 500,
      right: 40,
      area: false,
      target: document.getElementById(selector),
      mouseover: function(d, i) {
            var tf = d3.timeFormat('%b %e, %Y %H:%M:%S');
            var text = "";
	    if (data[i].label) {
              text += data[i].label;
            } 
            
            d3.select(`#${selector} svg .mg-active-datapoint`).html(
                `<tspan x='0' dy='1.2em'>${tf(d.time)} <tspan font-weight='bold' fill='red'>${formatter(d.value)}</tspan></tspan><tspan x='0' dy='1.2em'>${text}</tspan>`)
      }, 
      x_accessor: 'time',
      y_accessor: 'value',
      brush: 'xy'
  });
}

$(document).ready(function() {
  $.get("../metrics/api/list", function(metrics) {
    var node = $("#main");
    for (const metric of metrics) {
      node.append(`<div id="${metric}"></div>`);
      
      $.post(`../metrics/api/get/offset/${metric}`, "{}", function(data) {
        var polished = PrepareData(metric, data.point);
        ShowGraph(metric, polished);
        ShowLabels(metric, polished);
      }, "json");
    }
  });
});
