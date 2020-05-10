REQUIREMENTS
	lua-5.3 or >, libm (standard), jail (standard)
	PREFIX=/usr/local/lib/lua/5.3
	CFLAGS+= -I/usr/local/include/lua53 -fsanitize=safe-stack
	LDFLAGS+= -L/usr/local/lib

DESCRIPTION
	libluaknet.so - модуль lua для работы с сетевыми интерфейсами и шлюзом по умолчанию

FUNCTIONS
    arp_get([string ifname], [string hostname/ip]);
		описание:
			получить информацию из arp таблицы
		параметры:
			ifname - имя сетевого интерфейса по которому получить информацию
			hostname/ip - получить информацию по имени хоста или ip-адресу
		возврат:
			nil или таблица (пример 2, 3)

    arp_flush([string ifname]);
		описание:
			удалить данные из таблицы
		параметры:
			ifname - имя сетевого интерфейса по которому удалить информацию
		возврат:
			nil/true

    arp_delete(string hostname/ip);
		описание:
			удалить данные из таблицы
		параметры:
			hostname/ip - удалить информацию по имени хоста или ip-адресу
		возврат:
			nil/true

    create(string name);
		описание:
			создать интерфейс
		параметры:
			name - имя сетевого интерфейса (tap100) или тип (tap)
		возврат:
			nil/string - имя сетевого интерфейса 

    destroy(string ifname);
		описание:
			уничтожить интерфейс
		параметры:
			ifname - имя сетевого интерфейса
		возврат:
			nil/true

    vnet(string ifname, int/string jail);
		описание:
			отпроавить сетевой интерфейс в jail
		параметры:
			ifname - имя сетевого интерфейса
			jail - number jid или string jailname
		возврат:
			nil/true

    vnetd(string ifname, int/string jail);
		описание:
			вернуть сетевой интерфейс из jail
		параметры:
			ifname - имя сетевого интерфейса
			jail - number jid или string jailname
		возврат:
			nil/true

    setip(string ifname, int type, string ip, string netmask/prefix);
		описание:
			добавить ip-адрес на сетевой интерфейс
		параметры:
			ifname - имя сетевого интерфейса
			type - тип ip-адреса, 4 или 6
			ip - ip-адрес
			netmask/prefix - маска в формате ххх.ххх.ххх.ххх или префикс 0-128
		возврат:
			nil/true

    delip(string ifname, int type, string ip);
		описание:
			удалить ip-адрес на сетевом интерфейсе
		параметры:
			ifname - имя сетевого интерфейса
			type - тип ip-адреса, 4 или 6
			ip - ip-адрес
		возврат:
			nil/true

    clear(string ifname);
		описание:
			удалить все ip-адреса на сетевом интерфейсе
		параметры:
			ifname - имя сетевого интерфейса
		возврат:
			nil/true

    rename(string ifname, string new_ifname);
		описание:
			переименовать сетевой интерфейс
		параметры:
			ifname - имя сетевого интерфейса
			new_ifname - новое имя сетевого интерфейса
		возврат:
			nil/true

    setmac(string ifname, string mac);
		описание:
			установить mac-адрес сетевого интерфейса
		параметры:
			ifname - имя сетевого интерфейса
			mac - mac-адрес
		возврат:
			nil/true

    ifcap(string ifname, string opt1 [, string opt2 ...]);
		описание:
			включить или выключить возможности сетевого интерфейса
		параметры:
			ifname - имя сетевого интерфейса
			optN - возможности сетевого интерфейса, для отключения необходимо добавить '-' перед названием. например ifcap('em0', '-lro', '-tso', 'polling')
					lro, tso, rxcsum, txcsum, polling, toe, wol, netcons
		возврат:
			nil/true

    flags(string ifname, string flag1 [, string flag1 ...]);
		описание:
			включить или выключить флаги сетевого интерфейса
		параметры:
			ifname - имя сетевого интерфейса
			flagN - флаги сетевого интерфейса, для отключения необходимо добавить '-' перед названием (кроме up, down, normal). например flags('em0', 'up', '-promisc')
					up, down, arp, debug, promisc, link0, link1, link2, monitor, staticarp, normal, compress, noicmp
		возврат:
			nil/true

    list();
		описание:
			получить список сетевых интерфейсов и их статус
		возврат:
			nil или таблица { ['ifname1'] = 0/1, ['ifname2'] = 0/1 }

    info([string ifname]);
		описание:
			получить информацию о сетевом интерфейсе
		параметры:
			ifname - имя сетевого интерфейса, если не указывать то информация будет обо всех сетевых интерфейсах
		возврат:
			nil или таблица (пример 1)

    route_add_static4(int type, string dest, string gateway [, int fibnum]);
		описание:
			добавить статический шлюз IPv4
		параметры:
			type - тип адреса шлюза(1 - хост, 0 - сеть)
			dest - ip-адрес назначения
			gateway - адрес шлюза
		возврат:
			nil/true

    route_del_static4(int type, string dest, string gateway);
		описание:
			удалить статический шлюз IPv4
		параметры:
			type - тип адреса шлюза(1 - хост, 0 - сеть)
			dest - ip-адрес назначения
			gateway - адрес шлюза
		возврат:
			nil/true

    route_add(int type, string gateway [, int fibnum]);
		описание:
			добавить шлюз по умолчанию
		параметры:
			type - тип адреса шлюза, 4 или 6
			gateway - адрес шлюза
		возврат:
			nil/true

    route_del(int type);
		описание:
			удалить шлюз по умолчанию
		параметры:
			type - тип адреса шлюза, 4 или 6
		возврат:
			nil/true

    route_get();
		описание:
			получить список всех шлюзов
		возврат:
			nil или таблица (пример 4)

EXAMPLES
	Пример 1. list();
		Листинг:
			#!/usr/local/bin/lua53
			n = require('libluaknet');
			t = n.info('vtnet1');
			for k,v in pairs(t) do
			        print(k)
			        for k1,v1 in pairs(v) do
			                print("\t" .. k1, v1)
			                if (k1 == "groups") then
			                        for k2,v2 in pairs(v1) do
			                                print("\t\t\t" .. v2)
			                        end
			                end
			                if (k1 == "flags") then
			                        for k2,v2 in pairs(v1) do
			                                print("\t\t\t" .. v2)
			                        end
			                end
			                if (k1 == "nd6") then
			                        for k2,v2 in pairs(v1) do
			                                print("\t\t\t" .. v2)
			                        end
			                end
			                if (k1 == "ip4") then
                                    for k2,v2 in pairs(v1) do
                                            print("\t\t\t" .. v2.ip .. " " .. v2.netmask .. " " .. v2.broadcast)
                                    end
                            end
                            if (k1 == "ip6") then
                                    for k2,v2 in pairs(v1) do
                                            print("\t\t\t" .. v2.ip .. " " .. v2.prefix)
                                    end
                            end

			        end
			end

		Вывод:
			vtnet1
				mtu	1500
				flags	table: 0x3ba38c15b40
						BROADCAST
						PROMISC
						SIMPLEX
						MULTICAST
				nd6	table: 0x3ba38c15b80
						PERFORMNUD
						IFDISABLED
						AUTO_LINKLOCAL
				groups	table: 0x3ba38c15b00
				metric	0
				mac	00:a0:98:aa:11:8b
				ip6	table: 0x60293615bc0
						::12 64
						fe80::2a0:98ff:fea3:74be 64
				ip4	table: 0x60293615b40
						6.6.6.172 255.255.255.0 6.6.6.255

	Пример 2. arp_get(string ifname);
		Листинг:
			#!/usr/local/bin/lua53
			s = require('libluaknet')
			t = s.arp_get('tap0');
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

	Пример 3. arp_get(nil, string ip);
		Листинг:
			#!/usr/local/bin/lua53
			s = require('libluaknet')
			t = s.arp_get(nil, '6.6.6.172');
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

	Пример 4. route_get();
		Листинг:
			#!/usr/local/bin/lua53
			s = require('libluaknet')
			t = s.route_get();
			print('IP4');
			for k1,v1 in pairs(t.ipv4) do
				print('\t', v1.dest, v1.gtw, v1.ifname)
			end;
			print('IP6');
			for k1,v1 in pairs(t.ipv6) do
				print('\t', v1.dest, v1.gtw, v1.ifname)
			end

		Вывод:
		IP4
				default		6.6.6.222	vtnet0
				4.4.4.0/24	6.6.6.82	vtnet0
				6.6.6.0/24	link#1		vtnet0
				6.6.6.172	link#1		lo0
				127.0.0.1	link#2		lo0
		IP6
				::/96				::1			lo0
				::1					link#2		lo0
				::ffff:0.0.0.0/96	::1			lo0
				fe80::/10			::1			lo0
				fe80::/64			link#2		lo0
				fe80::1				link#2		lo0
				ff02::/16			::1			lo0