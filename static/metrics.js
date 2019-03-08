'use strict';

function ShowGraph(selector, data) {
  // %Q -> milliseconds since epoch, %s -> seconds since epoch.
  // https://github.com/d3/d3-time-format/blob/master/README.md#timeFormat
  var converter = d3.timeParse('%Q');
  data = data.map(function(t) { t.time = converter(t.time); return t; });

  MG.data_graphic({
      title: selector,
      description: "",
      data: data,
      width: 800,
      height: 300,
      right: 40,
      area: false,
      target: document.getElementById(selector),
      x_accessor: 'time',
      y_accessor: 'value'
  });
}

$(document).ready(function() {
  $.get("../metrics/api/list", function(metrics) {
    var node = $("#main");
    for (const metric of metrics) {
      node.append(`<div id="${metric}"></div>`);
      
      $.post(`../metrics/api/get/offset/${metric}`, "{}", function(data) {
        console.log(data);
        ShowGraph(metric, data.point);
      }, "json");
    }
  });
 
  d3.json('data/fake_users1.json', function(data) {
    data = MG.convert.date(data, 'date');
    MG.data_graphic({
        title: "Line Chart",
        description: "This is a simple line chart. You can remove the area portion by adding area: false to the arguments list.",
        data: data,
        width: 600,
        height: 200,
        right: 40,
        target: document.getElementById('fake_users1'),
        x_accessor: 'date',
        y_accessor: 'value'
    });
  });
});
