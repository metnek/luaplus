REQUIREMENTS
	lua-5.3 or >, libm (standard)
	PREFIX=/usr/local/lib/lua/5.3
	CFLAGS+= -I/usr/local/include/lua53 -fsanitize=safe-stack
	LDFLAGS+= -L/usr/local/lib

DESCRIPTION
	libluaarp.so - модуль lua для работы с ARP таблицами

FUNCTIONS
    get([string ifname], [string hostname/ip]);
		описание:
			получить информацию из arp таблицы
		параметры:
			ifname - имя сетевого интерфейса по которому получить информацию
			hostname/ip - получить информацию по имени хоста или ip-адресу
		возврат:
			nil или таблица (пример 1, 2)

    flush([string ifname]);
		описание:
			удалить данные из таблицы
		параметры:
			ifname - имя сетевого интерфейса по которому удалить информацию
		возврат:
			nil/true

    delete(string hostname/ip);
		описание:
			удалить данные из таблицы
		параметры:
			hostname/ip - удалить информацию по имени хоста или ip-адресу
		возврат:
			nil/true

EXAMPLES
	Пример 1. get(string ifname);
		Листинг:
			#!/usr/local/bin/lua53
			s = require('libluaarp')
			t = s.get('tap0');
			for k,v in pairs(t) do
				print(k);
				for k1,v1 in pairs(v) do
					print('\t', k1,v1)
				end
			end

		Вывод:
			1
				hostname	?
				mac			00:bd:a7:f4:02:00
				type		ethernet
				ipaddr		5.5.5.5
				permanent	1.0
				ifname		tap0

	Пример 2. get(nil, string ip);
		Листинг:
			#!/usr/local/bin/lua53
			s = require('libluaarp')
			t = s.get(nil, '6.6.6.172');
			for k,v in pairs(t) do
				print(k);
				for k1,v1 in pairs(v) do
					print('\t', k1,v1)
				end
			end

		Вывод:
			1
				permanent	true
				ipaddr		6.6.6.172
				ifname		vtnet0
				mac			00:a0:98:a3:74:be
				hostname	?
				type		ethernet
