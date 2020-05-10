REQUIREMENTS
	lua-5.3 or >, libkcs-kzfs > 3.0.0, libm (standard)
	PREFIX=/usr/local/lib/lua/5.3
	CFLAGS+= -I/usr/local/include/lua53 -fsanitize=safe-stack
	LDFLAGS+= -L/usr/local/lib

DESCRIPTION
	libluazfs.so - модуль lua для работы с ZFS

FUNCTIONS
	ZFS
		ds_prop_remove(string dataset_name, string/table prop[s]);
			описание:
				удалить пользовательский параметр из датасета
			параметры:
				dataset_name - имя датасета или volume
				prop[s] - имя параметра (string) или таблица с именами параметров (table), например {'prop1', 'prop2'}
			возврат:
				nil/true

		ds_destroy(string dataset_name [, int recursive]);
			описание:
				уничтожить датасет или volume
			параметры:
				dataset_name - имя датасета или volume
				recursive - уничтожить рекурсивно (0/1 выключено/включено, по умолчанию 0)
			возврат:
				nil/true

	    ds_rename(string dataset_name, string new_name [, int force [, int noumount [, int parents]]]);
			описание:
				переименовать датасет или volume
			параметры:
				dataset_name - имя датасета или volume
				new_name - новое имя датасета или volume
				force - Принудительно размонтировать любые файловые системы, которые должны быть размонтированы при переименовании. (0/1 выключено/включено, по умолчанию 0)
				noumount - Не перемонтировать файловые системы при переименовании. (0/1 выключено/включено, по умолчанию 0)
				parents - Создает все несуществующие родительские наборы данных. (0/1 выключено/включено, по умолчанию 0)
			возврат:
				nil/true

	    ds_create(string dataset_name [, table props]);
			описание:
				создать датасет или volume
			параметры:
				dataset_name - имя датасета или volume
				props - таблица параметров, необязательный параметр (prop_name (key) = value)
			возврат:
				nil/true

	    ds_update(string dataset_name, table props);
			описание:
				обновить параметры датасета или volume
			параметры:
				dataset_name - имя датасета или volume
				props - таблица параметров (prop_name (key) = value)
			возврат:
				nil/true

	    ds_list([string dataset_name,] table props);
			описание:
				получить список датасетов или volume'ов. если указать dataset_name, то работает как zfs get
			параметры:
				dataset_name - имя датасета или volume, необязательный параметр
				props - таблица имен параметров (prop_name1, prop_name2 ...)
			возврат:
				nil или таблица {dataset_name1 = {['prop1']='value', ['prop2']='value'}, dataset_name2 = {['prop1']='value', ['prop2']='value'}}

	    snap_destroy(string dataset_name, string snap_name [, int recursive]);
			описание:
				уничтожить снимок
			параметры:
				dataset_name - имя датасета или volume
				snap_name - имя снимка
				recursive - уничтожить рекурсивно (0/1 выключено/включено, по умолчанию 0)
			возврат:
				nil/true

	    snap_create(string dataset_name, string snap_name [, string desc [, int recursive]]);
			описание:
				создать снимок
			параметры:
				dataset_name - имя датасета или volume
				snap_name - имя снимка
				desc - описание снимка, если не указывать то "".
				recursive - создать рекурсивно (0/1 выключено/включено, по умолчанию 0)
			возврат:
				nil/true

	    snap_rollback(string dataset_name, string snap_name);
			описание:
				восстановить снимок
			параметры:
				dataset_name - имя датасета или volume
				snap_name - имя снимка
			возврат:
				nil/true

	    snap_list(string dataset_name);
			описание:
				обновить параметры датасета или volume
			параметры:
				dataset_name - имя датасета или volume
			возврат:
				nil или таблица {snap_name = {['creation'] = 'дата создания', ['desc'] = 'описание', ['used'] = 'размер'}, ...}

	    snap_rename(string dataset_name, string snap_name, string new_snap_name [, int force [, int recursive]]);
			описание:
				восстановить снимок
			параметры:
				dataset_name - имя датасета или volume
				snap_name - имя снимка
				new_snap_name - новое имя снимка
				force - восстановить принудительно (0/1 выключено/включено, по умолчанию 0)
				recursive - восстановить рекурсивно (0/1 выключено/включено, по умолчанию 0)
			возврат:
				nil/true

	    snap_update(string dataset_name, string snap_name [, string desc]);
			описание:
				обновить описание снимка
			параметры:
				dataset_name - имя датасета или volume
				snap_name - имя снимка
				desc - описание снимка, если не указывать то "".
			возврат:
				nil/true

	    send(string dataset_name, string ip, int port [, int move]);
			описание:
				отправить датасет на другой хост, принимающий хост сначала должен запустить функцию recv()
			параметры:
				dataset_name - имя датасета или volume
				ip - ip-адрес удаленного хоста
				port - порт удаленного хоста
				move - удалить после отправки (0/1 выключено/включено, по умолчанию 0)
			возврат:
				nil/true

	    recv(string dataset_name);
			описание:
				принять датасет с другого хоста
			параметры:
				dataset_name - имя датасета или volume
				ip - ip-адрес данного хоста
				port - порт данного хоста
			возврат:
				nil/true

	    settings_merge(string from, string to);
			описание:
				заменить пользовательские параметры в $to на параметры с $from
			параметры:
				from - имя датасета или полное имя снимка (dataset@snapname) откуда взять параметры
				to - имя датасета или полное имя снимка (dataset@snapname) где перезаписать параметры
			возврат:
				nil/true

	    mount_list();
			описание:
				получить список примонтиваронных датасетов и точки монтирования
			возврат:
				nil или таблица {['dataset_name1'] = 'mountpoint1', ['dataset_name2'] = 'mountpoint2'}

	ZPOOL
	    zpool_create(string zpool_name, table devs, table props, table fs_props);
			описание:
				создать зпул. props и fs_props - необязательные параметры, вместо них можно отправить в функцию nil
			параметры:
				zpool_name - имя зпула
				devs - таблица устройств зпула
						{
							['1'] = {
								['type'] = 'mirror/raidz/stripe...',
								['is_log'] = 0/1,
								['devs'] = {
									['1'] = 'md1',
									['2'] = 'md2', ...
								}
							}, ...
						}
				props - таблица параметров зпула, {['prop1']='value', ['prop2']='value' ...}
					также дополнительные параметры, которые у команды zpool являются аргументами (-f, -F ...). если параметр не нужен, то просто не добавлять в таблицу props
						['force'] = 'true' принудительное создание
						['no_feat'] = 'true' отключить дополнительные плюшки
						['root'] = 'string' альтернативный корневой каталог, параметр cachefile автоматически ставится в 'none'
						['mounpoint'] = 'string' точка монтирования
						['temp_name'] = 'string' временное имя зпула, параметр cachefile автоматически ставится в 'none'

				fs_props - таблица параметров zfs, {['prop1']='value', ['prop2']='value' ...}
			возврат:
				nil/true

	    zpool_destroy(string zpool_name);
			описание:
				уничтожить зпул
			параметры:
				zpool_name - имя зпула
			возврат:
				nil/true

	    zpool_update(string zpool_name, table props);
			описание:
				обновить параметры зпула
			параметры:
				zpool_name - имя зпула
				props - таблица параметров зпула, {['prop1']='value', ['prop2']='value' ...}
			возврат:
				nil/true

	    zpool_list(table props);
			описание:
				получить список зпулов.
			параметры:
				props - таблица имен параметров (prop_name1, prop_name2 ...)
			возврат:
				nil или таблица
						{
							guid1 = {
								['name']='zpool_name',
								['prop_name1']='value',
								['prop_name2']='value' ...
								},
							guid2 = {
								['name']='zpool_name',
								['prop_name1']='value',
								['prop_name2']='value' ...
							}
						}

	    zpool_export(string zpool_name);
			описание:
				экпортировать зпул
			параметры:
				zpool_name - имя зпула
			возврат:
				nil/true

	    zpool_clear(string zpool_name [, string dev [, int rewind]]);
			описание:
				удалить ошибки устройства в зпуле
			параметры:
				zpool_name - имя зпула
				dev - имя устройства, необязательный параметр (nil)
				rewind - Инициирует режим восстановления для неоткрытого пула (0/1 выключено/включено, по умолчанию 0),  необязательный параметр (nil)
			возврат:
				nil/true

	    zpool_reguid(string zpool_name);
			описание:
				создать новый уникальный идентификатор для зпула
			параметры:
				zpool_name - имя зпула
			возврат:
				nil/true

	    zpool_reopen(string zpool_name);
			описание:
				переоткрыть все vdevs, связанные с зпулом
			параметры:
				zpool_name - имя зпула
			возврат:
				nil/true

	    zpool_labelclear(string dev [, int force]);
			описание:
				удалить информацию метки ZFS с указанного устройства.
			параметры:
				dev - имя устройства
				force - принудительное удаление (0/1 выключено/включено, по умолчанию 0)
			возврат:
				nil/true

	    zpool_attach(string zpool_name, string dev, string new_dev [, int force]);
			описание:
				присоединить диск к диску или к raid или к mirror
			параметры:
				zpool_name - имя зпула
				dev - имя устройства к которому необходимо присоединить
				new_dev - имя устройства которое необходимо присоединить
				force - принудительное присоединение (0/1 выключено/включено, по умолчанию 0)
			возврат:
				nil/true

	    zpool_replace(string zpool_name, string dev, string new_dev [, int force]);
			описание:
				заменить устройство в зпуле
			параметры:
				zpool_name - имя зпула
				dev - имя устройства, которое необходимо заменить
				new_dev - имя нового устройства, nil когда устройство вышло из строя и на его место добавляют новое, то есть имя осталось прежним
				force - принудительная замена (0/1 выключено/включено, по умолчанию 0)
			возврат:
				nil/true

	    zpool_detach(string zpool_name, string dev);
			описание:
				отсоединить диск от зпула
			параметры:
				zpool_name - имя зпула
				dev - имя устройства, которое необходимо отсоединить
			возврат:
				nil/true

	    zpool_online(string zpool_name, table dev [, int expand]);
			описание:
				перевести устройство в режим онлайн
			параметры:
				zpool_name - имя зпула
				dev - таблица устройств {'md1', 'md2', ...}
				expand - расширить устройство (0/1 выключено/включено, по умолчанию 0)
			возврат:
				nil/true

	    zpool_offline(string zpool_name, table dev [, int temp]);
			описание:
				перевести устройство в режим офлайн
			параметры:
				zpool_name - имя зпула
				dev - таблица устройств {'md1', 'md2', ...}
				temp - временный перевод в режим офлайн, после перезагрузки хоста устройствавернутся в прежний режим (0/1 выключено/включено, по умолчанию 0)
			возврат:
				nil/true

	    zpool_add(string zpool_name, table devs [, int force]);
			описание:
				добавить устройства в зпул
			параметры:
				zpool_name - имя зпула
				devs - таблица устройств зпула
						{
							['1'] = {
								['type'] = 'mirror/raidz/stripe...',
								['is_log'] = 0/1,
								['devs'] = {
									['1'] = 'md1',
									['2'] = 'md2', ...
								}
							}, ...
						}
				force - принудительное добавление (0/1 выключено/включено, по умолчанию 0)
			возврат:
				nil/true

	    zpool_scrub(string zpool_name, int cmd);
			описание:
				сканирование зпула
			параметры:
				zpool_name - имя зпула
				cmd - команда сканирования (по умолчанию 0)
					0 - старт сканирования (start)
					1 - приостановить сканирование (pause)
					2 - остановить сканирование (stop)
			возврат:
				nil/true

	    zpool_split(string zpool_name, string new_zpool_name, table props, table dev);
			описание:
				создать новый зпул из дисков старого зпула. старый зпул должен состоять только из mirror устройств,
				из каждого mirror устройства берется один диск (по умолчанию последний) и из этих дисков создается зпул.
			параметры:
				zpool_name - имя зпула
				new_zpool_name - имя нового зпула
				props - таблица параметров зпула, {['prop1']='value', ['prop2']='value' ...}, может быть nil
					также дополнительные параметры, которые у команды zpool являются аргументами (-f, -F ...). если параметр не нужен, то просто не добавлять в таблицу props
						'mntopts' = 'param1,param2' параметры монтирования через запятую
								atime/noatime
								exec/noexec
								ro/rw
								suid/nosuid
						'root' = 'string' альтернативный корневой каталог
				dev - таблица устройств {'md1', 'md2', ...}, может быть nil
			возврат:
				nil/true

	    zpool_get_devs(string zpool_name);
			описание:
				получить список устройств зпула
			параметры:
				zpool_name - имя зпула
			возврат:
				nil или таблица
						{
							['1'] = {
								['type'] = 'mirror/raidz/stripe...',
								['is_log'] = 0/1,
								['devs'] = {
									['1'] = 'md1',
									['2'] = 'md2', ...
								}
							}, ...
						}

	    zpool_status(string zpool_name);
			описание:
				показать статус зпула
			параметры:
				zpool_name - имя зпула
			возврат:
				nil или таблица (пример 1 ниже)

	    zpool_import_list([string cachefile][, string search][, int destroyed]);
			описание:
				получить список зпулов для импорта. все параметры являются необязательными
			параметры:
				cachefile - загрузить информацию из кэш файла
				searchdirs - искать устройства в данных папках
				destroyed - показать только уничтоженные зпулы
			возврат:
				nil или таблица (пример 2 ниже)

	    zpool_import(string zpool_name [, table props]);
			описание:
				импортировать зпул
			параметры:
				zpool_name - имя зпула
				props - необязательный параметр. таблица параметров зпула, {['prop1']='value', ['prop2']='value' ...}
					также дополнительные параметры, которые у команды zpool являются аргументами (-f, -F ...). если параметр не нужен, то просто не добавлять в таблицу props
						['newname'] = 'string' импортировать с новым именем
						['temp_name'] = 'true' -использовать вместе с параметром newname. если ['temp_name']='true', то новое имя будет использоваться до следующего экспорта
						['force'] = 'true' принудительное импортирование
						['rewind'] = 'true' восстановление испорченного зпула
						['missing_log'] = 'true' разрешить импортирование без log устройства
						['no_mount'] = 'true' импортировать без монтирования 
						['mntopts'] = 'param1,param2' параметры монтирования через запятую
								atime/noatime
								exec/noexec
								ro/rw
								suid/nosuid
						['root'] = 'string' альтернативный корневой каталог. исключает cachefile
						['cachefile'] = 'string'  загрузить информацию из кэш файла. исключает searchdirs
						['searchdirs'] = 'dir1,dir2...' список папок через запятую, искать устройства в данных папках. исключает cachefile
						['destroyed'] = 'true' импортировать только уничтоженные зпулы, также необходим параметр ['force']=true
			возврат:
				nil/true

	    zpool_import_all([table props]);
			описание:
				импортировать все зпулы
			параметры:
				props - таблица параметров зпула, {['prop1']='value', ['prop2']='value' ...}
					также дополнительные параметры, которые у команды zpool являются аргументами (-f, -F ...). если параметр не нужен, то просто не добавлять в таблицу props
						['force'] = 'true' принудительное импортирование
						['rewind'] = 'true' восстановление испорченного зпула
						['missing_log'] = 'true' разрешить импортирование без log устройства
						['no_mount'] = 'true' импортировать без монтирования 
						['mntopts'] = 'param1,param2' параметры монтирования через запятую
								atime/noatime
								exec/noexec
								ro/rw
								suid/nosuid
						['root'] = 'string' альтернативный корневой каталог. исключает cachefile
						['cachefile'] = 'string'  загрузить информацию из кэш файла. исключает searchdirs
						['searchdirs'] = 'dir1,dir2...' список папок через запятую, искать устройства в данных папках. исключает cachefile
						['destroyed'] = 'true' импортировать только уничтоженные зпулы, также необходим параметр ['force']=true
			возврат:
				nil/true

EXAMPLES
	Пример 1. zpool_status();
		Листинг:
			#!/usr/local/bin/lua53

			zfs = require('libluazfs')
			q = zfs.zpool_status('test1');

			print("name: " .. q.name)
			print("guid: " .. q.guid)
			print("state: " .. q.state)
			print("status: " .. (q.status or "nil"))
			print("scan: " .. (q.scan or "nil"))
			print("scan_warn: " .. (q.scan_warn or "nil"))
			print("remove: " .. (q.remove or "nil"))
			print("checkpoint: " .. (q.checkpoint or "nil"))
			print()

			print("\t\t\tstate\trbuf\twbuf\tcbuf\tmsg\taction\tpath")
			for k,v in pairs(q.config) do
				print("config")
			        print("\t" .. v.name .. "\t" .. v.state, v.rbuf, v.wbuf, v.cbuf, (v.msg or "nil"), (v.act or "nil"), (v.config_path or "nil"))
				if (v.config_list) then
					for k1, v1 in pairs(v.config_list) do
						print("\t\t" .. v1.name,v1.state, v1.rbuf, v1.wbuf, v1.cbuf, (v1.msg or "nil"), (v1.act or "nil"), (v1.config_path or "nil"))
					end
				end

			end
			if (q.logs) then
				print("logs")
				for k,v in pairs(q.logs) do
					print("\t" .. v.name .. "\t" .. v.state, v.rbuf, v.wbuf, v.cbuf, (v.msg or "nil"), (v.act or "nil"), (v.config_path or "nil"))
					if (v.config_list) then
						for k1, v1 in pairs(v.config_list) do
							print("\t\t" .. v1.name,v1.state, v1.rbuf, v1.wbuf, v1.cbuf, (v1.msg or "nil"), (v1.act or "nil"), (v1.config_path or "nil"))
						end
					end

				end
			end
			if (q.cache) then
				print("cache")
				for k,v in pairs(q.cache) do
					print("\t" .. v.name .. "\t" .. v.state, v.rbuf, v.wbuf, v.cbuf, (v.msg or "nil"), (v.act or "nil"), (v.config_path or "nil"))
					if (v.config_list) then
						for k1, v1 in pairs(v.config_list) do
							print("\t\t" .. v1.name,v1.state, v1.rbuf, v1.wbuf, v1.cbuf, (v1.msg or "nil"), (v1.act or "nil"), (v1.config_path or "nil"))
						end
					end
				end
			end
			if (q.spares) then
				print("spares")
				for k,v in pairs(q.spares) do
					print("\t" .. v.name .. "\t" .. v.state, v.rbuf, v.wbuf, v.cbuf, (v.msg or "nil"), (v.act or "nil"), (v.config_path or "nil"))
					if (v.config_list) then
						for k1, v1 in pairs(v.config_list) do
							print("\t\t" .. v1.name,v1.state, v1.rbuf, v1.wbuf, v1.cbuf, (v1.msg or "nil"), (v1.act or "nil"), (v1.config_path or "nil"))
						end
					end
				end
			end

		Вывод:
			name: test
			guid: 16135579698092781428
			state: ONLINE
			status: nil
			scan: none requested
			scan_warn: nil
			remove: nil
			checkpoint: nil

						state	rbuf	wbuf	cbuf	msg	action	path
			config
				md1	ONLINE	0	0	0	nil	nil	nil
			config
				mirror-1	ONLINE	0	0	0	nil	nil	nil
					md2	ONLINE	0	0	0	nil	nil	nil
					md3	ONLINE	0	0	0	nil	nil	nil
			logs
				mirror-2	ONLINE	0	0	0	nil	nil	nil
					md4	ONLINE	0	0	0	nil	nil	nil
					md5	ONLINE	0	0	0	nil	nil	nil


	Пример 2. zpool_import_list()
		Листинг:
			#!/usr/local/bin/lua53

			zfs = require('libluazfs')
			q = zfs.zpool_import_list();

			for k,v in pairs(q) do
			        print("name: " .. v.name)
			        print("guid: " .. v.guid)
			        print("state: " .. v.state)
			        print("action: " .. v.action)
			        print("destroyed: " .. v.destroyed)

			        print("comment: " .. (v.comment or "nil"))
			        print("msg: " .. (v.msg or "nil"))
			        print("warn: " .. (v.warn or "nil"))
			        for k1,v1 in pairs(v.config) do
			                print("\t" .. v1.name, v1.state)
			                if (v1.config_list) then
			                        for key, val in pairs(v1.config_list) do
			                                print("\t\t" .. val.name, val.state)
			                        end
			                end
			        end
			        print("logs")
			        for k1,v1 in pairs(v.logs) do
			                print("\t" .. v1.name, v1.state)
			                if (v1.config_list) then
			                        for key, val in pairs(v1.config_list) do
			                                print("\t\t" .. val.name, val.state)
			                        end
			                end
			        end
			        print()
			end

		Вывод:
			name: test
			guid: 16135579698092781428
			state: ONLINE
			action: The pool can be imported using its name or numeric identifier.
			destroyed: 0
			comment: nil
			msg: nil
			warn: nil
				md1	ONLINE
				mirror-1	ONLINE
					md2	ONLINE
					md3	ONLINE
			logs
				mirror-2	ONLINE
					md4	ONLINE
					md5	ONLINE

	Пример 3. zpool_get_devs()
		Листинг:
			#!/usr/local/bin/lua53

			zfs = require('libluazfs')
			q = zfs.zpool_get_devs('test')

			for k,v in pairs(q) do

				print(v.type)
				print("log:" .. v.is_log)
				for key,val in pairs(v.devs) do
					print("  " .. val)
				end
				print()
			end

		Вывод:
			mirror-1
			log:0
			  md2
			  md3

			mirror-2
			log:1
			  md4
			  md5

			stripe
			log:0
			  md1
