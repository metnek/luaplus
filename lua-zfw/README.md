REQUIREMENTS
	lua-5.3 or >, libkcs-zfw, libutil, libm (standard)
	PREFIX=/usr/local/lib/lua/5.3
	CFLAGS+= -I/usr/local/include/lua53 -fsanitize=safe-stack
	LDFLAGS+= -L/usr/local/lib

DESCRIPTION
	libluazfw.so - модуль lua для работы с ip firewall

FUNCTIONS
    rules_list(int show_stats, int set);
		описание:
			показать список правил файервола
		параметры:
			show_stats - показать статистику пакетов в правилах
			set - группа правил от 0 до 31, если все то -1 или nil
		возврат:
			nil или таблица (пример 1)

    rule_add(string/table rule);
		описание:
			добавить одно или несколько правил
		параметры:
			rule - если одно правило то строка вида "100 allow ip from any to any"
					если несколько то таблица вида:
					 { 
					 	[1]="200 allow tcp from me to any",
					 	[2]="300 deny tcp from any to me",
					 	...
					 }
		возврат:
			nil/true

    rule_delete(int set, int begin [, int end]);
		описание:
			удалить одно или несколько правил
		параметры:
			set - группа правил от 0 до 31
			begin - номер с которого удалить правила
			end - номер до которого удалить правила. если не указывать то удалится одно правило с номером begin
		возврат:
			nil/true

    rule_move(int num, int new_set);
		описание:
			переместить правило в другую группу
		параметры:
			num - номер правила
			set - новая группа правил от 0 до 31
		возврат:
			nil/true

    tables_list();
		описание:
			получить список таблиц файервола
		возврат:
			nil таблица { tablename1 = { [1] = '1.1.1.0/24', [2] = '2.2.2.2/32' }, tablename2 = { [1] = '3.3.3.0/24'} }

    tables_destroy();
		описание:
			удалить все таблицы файервола
		возврат:
			nil/true

    table_create(string tablename);
		описание:
			создать новую таблицу файервола
		параметры:
			tablename - имя новой таблицы
		возврат:
			nil/true

    table_destroy(string tablename);
		описание:
			удалить таблицу файервола
		параметры:
			tablename - имя таблицы
		возврат:
			nil/true

    table_add(string tablename, string entry);
		описание:
			добавить запись в таблицу файервола
		параметры:
			tablename - имя таблицы
			entry - новая запись
		возврат:
			nil/true

    table_delete(string tablename, string entry);
		описание:
			удалить запись из таблицы файервола
		параметры:
			tablename - имя таблицы
			entry - запись таблицы
		возврат:
			nil/true

    table_get(string tablename);
		описание:
			получить все записи из таблицы файервола
		параметры:
			tablename - имя таблицы
		возврат:
			nil или таблица { [1] = '1.1.1.0/24', [2] = '2.2.2.2/32'}

    table_flush(string tablename);
		описание:
			удалить все записи из таблицы файервола
		параметры:
			tablename - имя таблицы
		возврат:
			nil/true

    set_delete(int set_num);
		описание:
			удалить все правила из сета (группы правил)
		параметры:
			set_num - номер сета [0-31]
		возврат:
			nil/true

    set_list(int set_num);
		описание:
			получить список сетов [0-31].
			если значение true, то значит что сет включен. иначе выключен
		возврат:
			nil или таблица { [0] = true, [1]=false, [2] = false, [3] = true, ... [31] =  true}

    set_enable(int set_num);
		описание:
			включить все правила из сета (группы правил)
		параметры:
			set_num - номер сета [0-31]
		возврат:
			nil/true

    set_disable(int set_num);
		описание:
			выключить все правила из сета (группы правил)
		параметры:
			set_num - номер сета [0-31]
		возврат:
			nil/true

    set_move(int set_from, int set_to);
		описание:
			переместить все правила из сета set_from в сет set_to
		параметры:
			set_from - номер сета [0-31] откуда переместить
			set_to - номер сета [0-31] куда переметить
		возврат:
			nil/true

    pipe_delete(int pipe_num);
		описание:
			удалить конфигурацию pipe с номером pipe_num
		параметры:
			pipe_num - номер pipe
		возврат:
			nil/true

    queue_delete(int queue_num);
		описание:
			удалить конфигурацию queue с номером queue_num
		параметры:
			queue_num - номер queue
		возврат:
			nil/true

    nat_delete(int nat_num);
		описание:
			удалить конфигурацию nat с номером nat_num
		параметры:
			nat_num - номер nat
		возврат:
			nil/true

    pipe_config(int pipe_num, string config);
		описание:
			добавить конфигурацию pipe
		параметры:
			pipe_num - номер pipe
			config - конфигурация pipe, например pipe_config(123, 'bw 100Kbit/s queue 20')
		возврат:
			nil/true

    queue_config(int queue_num, string config);
		описание:
			добавить конфигурацию queue
		параметры:
			queue_num - номер queue
			config - конфигурация queue, например queue_config(123, 'pipe 1 weight 50 queue 20')
		возврат:
			nil/true

    nat_config(int nat_num, string config);
		описание:
			добавить конфигурацию nat
		параметры:
			nat_num - номер nat
			config - конфигурация nat, например nat_config(123, 'if em0')
		возврат:
			nil/true

EXAMPLES
	Пример 1. list();
		Листинг:
			#!/usr/local/bin/lua53
			zfw = require('libluazfw')
			t = zfw.rules_list()
			for k,v in pairs(t) do
				print(k)
				for k1,v1 in pairs(v) do
					print(k1,v1)
				end
			end

		Вывод:
			1
				disabled	false
				rule_num	1
				rule		allow ip from any to any
				set			0
			2
				disabled	false
				rule_num	123
				rule		allow tcp from me to any
				set			0
			3
				disabled	false
				rule_num	380
				rule		deny tcp from any to 9.8.7.123 via vtnet2
				set			0
			4
				disabled	false
				rule_num	65535
				rule		deny ip from any to any
				set			31
