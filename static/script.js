"use strict";

$(document).ready(function () {

  const receipt_data_url = '/receipt';
  const receipt_page_url = '/receipt_page.html';
  const order_post_url = '/order';
  const currency_formatter = new Intl.NumberFormat(
    'de-DE', { style: 'currency', currency: 'EUR', }
  );

  class Pizza {

    constructor(name, id, price) {
      this.name = name;
      this.id = id;
      this.price = price;
    }

    static fromSelection(selection) {
      try {
        let id = parseInt(selection.val(), 10);
        if (!$.isNumeric(id))
          throw TypeError(`error parsing id from select value: ${selection.val()}`);
        let split = selection.text().split(' ', 2);
        let name = split[0];
        let price = parseFloat(split[1].replace(',', '.'));
        if (!$.isNumeric(price))
          throw TypeError(`error parsing price: ${split[1]}`);
        return new Pizza(name, id, price);
      } catch (e) {
        console.error(`error parsing selection item: ${e.message}`);
        return null;
      }
    }

    toStr() {
      return `<Pizza(name=${this.name}, id=${this.id}, price=${this.price}>`
    }
  }

  class Order {

    constructor(
      order_map, // int id -> [Pizza p, int count]
      order_list_div_selector,
      order_items_ul_selector,
      order_form_selector,
      currency_formatter
    ) {
      this.order_map = order_map;
      this.order_list_div_selector = order_list_div_selector;
      this.order_items_ul_selector = order_items_ul_selector;
      this.order_form_selector = order_form_selector;
      this.currency_formatter = currency_formatter;
    }

    static fromSelectOptions(
      option_selector,
      order_list_div_selector,
      order_items_ul_selector,
      order_form_selector,
      currency_formatter,
    ) {
      var order_map = new Map();
      for (let item of $(option_selector)) {
        if (item.value === "")
          continue;
        let pizza_id = parseInt(item.value, 10);
        let [pizza_name, pizza_price] = item.text.split(' ', 2);
        pizza_price = parseFloat(pizza_price.replace(',', '.'));
        if (!pizza_id || !$.isNumeric(pizza_price)) {
          console.error('error interpreting ' + item.html);
          continue;
        }
        let pizza = new Pizza(pizza_name, pizza_id, pizza_price);
        order_map.set(pizza.id, [pizza, 0]);
        console.debug('created: ' + pizza.toStr());
      }

      return new Order(
        order_map,
        order_list_div_selector,
        order_items_ul_selector,
        order_form_selector,
        currency_formatter,
      );
    }

    add(pizza) {
      if (!(pizza instanceof Pizza)) return;
      let [p, count] = this.order_map.get(pizza.id);
      this.order_map.set(p.id, [p, ++count]);
    }

    remove(pizza) {
      if (!(pizza instanceof Pizza)) return;
      let [p, count] = this.order_map.get(pizza.id);
      if (--count < 0)
        return;
      this.order_map.set(p.id, [pizza, count]);
    }

    static createOrderListItem(pizza, count, currency_formatter) {
      return [
        '<li class="list-group-item d-flex justify-content-between lh-condensed">',
          '<div>',
            '<h6 class="my-0">', pizza.name, '</h6>',
            '<small class="text-muted">',
              '<span>Anzahl : </span>',
              '<span id="item_count">', count, '</span>',
            '</small>',
          '</div>',
          '<span class="text-muted">', currency_formatter.format(pizza.price * count), '</span>',
        '</li>',
      ].join('');
    }

    static createInputField(pizza_id) {
      return $('<input>').attr({
        type: 'hidden',
        name: 'pizza_id',
        value: pizza_id,
      }).wrap('<li>');
    }

    updateHtml() {

      let total = 0.0;
      let item_count = 0;

      for (let [pizza, count] of this.order_map.values()) {
        let list_elem = $(`${this.order_items_ul_selector} :contains('${pizza.name}')`);

        if (count < 1 && list_elem.length < 1)
          continue;
        else if (count < 1 && list_elem.length >= 1)
          list_elem.remove();
        else if (count >= 1 && list_elem.length < 1) {
          let html = Order.createOrderListItem(pizza, count, this.currency_formatter);
          $(this.order_items_ul_selector).prepend(html);
        }
        else if (count >= 1 && list_elem.length >= 1)
          list_elem.replaceWith(Order.createOrderListItem(pizza, count, this.currency_formatter));

        total += count * pizza.price;
        item_count += count;
      }

      $(this.order_list_div_selector).find('span.badge').text(item_count);
      $(this.order_list_div_selector).find('strong').text(this.currency_formatter.format(total));
    }

    writeFormInputFields() {
      let form = $(this.order_form_selector);
      form.append($('<ul>').attr({
        id: 'dynamic_inputs',
      }));
      let ul = $('#dynamic_inputs');
      for (let [pizza, count] of this.order_map.values()) {
        if (count < 1) continue;
        for (let i = 0; i < count; i++) {
          ul.append(Order.createInputField(pizza.id));
        }
      }
    }
  }// class Order

  const parseJwtData = (token) => {
    try {
      let data = JSON.parse(atob(token.split('.')[1]));
      if (!('aud' in data) || !('iss' in data))
        return null;
      return data;
    } catch {
      console.log('parseReceiptData() failed: ' + e.message);
      return null;
    }
  };

  const parseReceiptData = (raw_json_str) => {
    try {
      let data = JSON.parse(raw_json_str);
      if (!('address' in data) || !('name' in data) || !('timestamp' in data) || !('order_items' in data))
        return null;
      for (let item of data.order_items)
        if (!('id' in item) || !('price' in item) || !('count' in item) || !('description' in item))
          return null;
      return data;
    } catch(e) {
      console.log('parseReceiptData() failed: ' + e.message);
      return null;
    }
  }

  const createReceiptLine = (pos, item_name, price, count, currency_formatter) => {
    return [
      '<tr>',
        '<td class="text-left">', pos, '</td>',
        '<td class="text-left strong">', item_name, '</td>',
        '<td class="text-right">', currency_formatter.format(price), '</td>',
        '<td class="text-right">', count, '</td>',
        '<td class="text-right">', currency_formatter.format(price * count), '</td>',
      '</tr>',
    ].join('');
  }

  var order = Order.fromSelectOptions(
    '#pizza_select option',
    '#order_list',
    '#order_items',
    '#order_form',
    currency_formatter,
  );

  $('#order_list_add').on('click', function () {
    let order_item = Pizza.fromSelection($('#pizza_select :selected'));
    if (order_item) {
      order.add(order_item);
      order.updateHtml();
    }
  });
  $('#order_list_remove').on('click', function () {
    let order_item = Pizza.fromSelection($('#pizza_select :selected'));
    if (order_item) {
      order.remove(order_item);
      order.updateHtml();
    }
  });
  $('#order_form').on('submit', function (event) {
    event.preventDefault();

    order.writeFormInputFields();


    // validate forms
    for (let form of document.getElementsByClassName('needs-validation')) {
      if (form.checkValidity() === false) {
        event.stopPropagation();
        form.classList.add('was-validated');
        return;
      }
      form.classList.add('was-validated');
    }

    function postOrder(form_data) {
      console.log('posting '+order_post_url);
      return $.ajax({
        url: order_post_url,
        method: 'POST',
        data: form_data, //hidden input fields
        error: function(res) {console.error('postOrder() failed: ' + res.responseStatus, + ' ', res.error + ' ' + res.responseText.slice(0,100))}
      })
    }

    function getReceiptData(post_order_response) {
      let token = post_order_response;
      let token_data = parseJwtData(token);
      if (!token_data) {
        console.error('token malformed');
        // todo set failed
      }
      let order_id = token_data.aud;
      let url_with_params = `${receipt_data_url}?order_id=${order_id}`;
      console.log('calling ' + url_with_params);
      return $.ajax({
        url: url_with_params,
        method: 'GET',
        headers: { Authorization: 'Bearer ' + token },
        dataType: 'text',
        error: function(res) {console.error('getReceiptData() failed: ' + res.responseStatus, + ' ', res.error + ' ' + res.responseText.slice(0,100))}
      })
    }

    function getReceiptPage() {
      console.log('getting ' + receipt_page_url);
      return $.ajax({
        url: receipt_page_url,
        method: 'GET',
        error: function(res) {console.error('getReceiptPage() failed: ' + res.responseStatus, + ' ', res.error + ' ' + res.responseText.slice(0,100))}
      })
    }

    let form_data = $(this).serialize();
    let receipt_data_promise = postOrder(form_data).then(getReceiptData);

    $.when(receipt_data_promise, getReceiptPage())
      .done(function (receipt_data_res, receipt_page_res) {

        // res := [ data, statusText, jqXHR ]

        let receipt_data = parseReceiptData(receipt_data_res[0]);
        if (!receipt_data) {
          console.error('receipt data malformed');
          return;
        }

        // switch out body to receipt
        $("body").html(receipt_page_res[0]);
        history.pushState({}, document.title, receipt_page_url);

        $('#customer_name').text(receipt_data.name);
        $('#customer_street').text(receipt_data.address);
        // TODO $('#customer_city').text();

        let tbody = $('#receipt_items_table').find('tbody');
        tbody.empty();
        let pos = 1;
        let subtotal = 0.0;
        for (let item of receipt_data.order_items) {
          tbody.append(
            createReceiptLine(
              pos++,
              item.description,
              item.price,
              item.count,
              currency_formatter
          ));
          subtotal += item.price * item.count;
        }

        let discount = 0;
        $('#subtotal').text(currency_formatter.format(subtotal));
        $('#discount').text(currency_formatter.format(discount));
        $('#total').text(currency_formatter.format(subtotal - discount));

      })
      .fail(function (receipt_data_res, receipt_page_res) {
        console.error('some server call failed, cant display receipt due to missing data');
        alert('some server call failed, cant display receipt due to missing data');
      });//when-block


  });// order_form on submit func
});// document.ready func