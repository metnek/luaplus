REQUIREMENTS
	lua-5.3 or >, libm (standard)
	PREFIX=/usr/local/lib/lua/5.3
	CFLAGS+= -I/usr/local/include/lua53 -fsanitize=safe-stack
	LDFLAGS+= -L/usr/local/lib

DESCRIPTION
	libluaspwd.so - модуль lua для работы с spwd.db

FUNCTIONS
    update(int uid, table info);
		описание:
			добавить пользователя
		параметры:
			uid - идентификатор пользователя
			info - новая информация о пользователе, если какой то параметр не меняется то просто не добавлять в таблицу
				name - имя пользователя
				gid - группа пользователя
				passwd - пароль пользователя
				shell - командная оболочка пользователя
				dir - домашняя папка пользователя (сама папка не создается, только запись в бд)
				gecos - описание пользователя
				class - user access class
				change - password change time
				expire - account expiration
		возврат:
			nil/true

    add(table info);
		описание:
			добавить пользователя
		параметры:
			info - информация о пользователе
				name - имя пользователя (обязательный параметр)
				gid - группа пользователя
				passwd - пароль пользователя
				shell - командная оболочка пользователя
				dir - домашняя папка пользователя (сама папка не создается, только запись в бд)
				gecos - описание пользователя
				class - user access class
				change - password change time
				expire - account expiration
		возврат:
			nil/true

    delete(int uid);
		описание:
			удалить пользователя
		параметры:
			uid - идентификатор пользователя
		возврат:
			nil/true

    get(string/int find);
		описание:
			получить информацию о пользователе
		параметры:
			find - идентификатор пользователя (uid) или его имя
		возврат:
			nil или таблица (пример 1, 2)

    get_all();
		описание:
			получить информацию обо всех пользователях
		возврат:
			nil или таблица (пример 3)

    verify(string username, string pwd);
		описание:
			проверить пароль пользователя
		параметры:
			username - имя пользователя
			pwd - пароль
		возврат:
			nil/true

EXAMPLES
	Пример 1. get(string name);
		Листинг:
			s = require('libluaspwd')
			t = s.get('alia')
			for k,v in pairs(t) do print(k,v) end

		Вывод:
			gecos	User &
			uid	1001
			gid	1001
			name	alia
			class	kekek
			dir	/home/alia
			shell	/bin/sh
			expire	0
			change	0

	Пример 2. get(int uid);
		Листинг:
			s = require('libluaspwd')
			t = s.get(1001)
			for k,v in pairs(t) do print(k,v) end

		Вывод:
			gecos	User &
			uid	1001
			gid	1001
			name	alia
			class	kekek
			dir	/home/alia
			shell	/bin/sh
			expire	0
			change	0

	Пример 3. get_all();
		Листинг:
			s = require('libluaspwd')
			t = s.get_all();
			for k,v in pairs(t) do
				print(k);
				for k1,v1 in pairs(v) do
					print("\t".. k1,v1)
				end
			end

		Вывод:
			1
				dir	/root
				name	root
				gid	0
				expire	0
				class	
				shell	/bin/csh
				change	0
				uid	0
				gecos	Charlie &
			2
				dir	/root
				name	toor
				gid	0
				expire	0
				class	
				shell	
				change	0
				uid	0
				gecos	Bourne-again Superuser
			3
				dir	/root
				name	daemon
				gid	1
				expire	0
				class	
				shell	/usr/sbin/nologin
				change	0
				uid	1
				gecos	Owner of many system processes
