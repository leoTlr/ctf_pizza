begin transaction;

create table if not exists 'order' (
    order_id     integer not null primary key autoincrement,
    address     text,
    name        text,
    timestamp   text
);

create table if not exists 'pizza' (
    pizza_id    integer not null primary key,
    price       real,
    description text
);

create table if not exists 'order_pizza' (
    order_id integer,
    pizza_id integer,
    constraint 'order' foreign key ('order_id') references 'order' ('order_id') on delete cascade,
    constraint 'pizza' foreign key ('pizza_id') references 'order' ('pizza_id') on delete cascade
);

commit;

begin transaction;

insert into pizza values (1, 4.5, 'Margherita');
insert into pizza values (2, 5.0, 'Salami');
insert into pizza values (3, 5.5, 'Thunfisch');
insert into pizza values (4, 6.0, 'Döner');
insert into pizza values (5, 6.5, 'Spezial');
insert into pizza values (6, 5.5, 'Zufall');

commit;
