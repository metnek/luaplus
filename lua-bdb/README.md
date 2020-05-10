REQUIREMENTS
	lua-5.3 or >, libkcs-bdb, libm (standard)
	PREFIX=/usr/local/lib/lua/5.3
	CFLAGS+= -I/usr/local/include/lua53 -fsanitize=safe-stack
	LDFLAGS+= -L/usr/local/lib
	kenv kcs.bdb.path="путь до папки с бд"

DESCRIPTION
	libluabdb.so - модуль lua для работы с Berkeley Database

FUNCTIONS
    create(string dbname, string path, ...);
		описание:
			создать бд
		параметры:
			string - имя бд
			string - путь до папки где создаьб бд. может быть nil, тогда путь будет взят из kenv
			... - колонки бд в виде colname:coltype
					coltype:
						s - строка
						l - целое число
						d - число с плавающей точкой
						S - массив строк
						L - массив целых чисел
						D - массив чисел с плавающей

		возврат:
			nil/true

    set(string dbname, string path, int key, table data);
		описание:
			добавить или изменить значение в бд
		параметры:
			string - имя бд
			string - путь до папки с бд. может быть nil, тогда путь будет взят из kenv
			key - ключ под которым будут добавлены/изменены данные
			data - данные в виде { ['column1'] = value1, ['column2'] = value2, ... }
		возврат:
			nil/true

	get_all(string dbname, string path)
		описание:
			получить все значения из бд
		параметры:
			string - имя бд
			string - путь до папки с бд. может быть nil, тогда путь будет взят из kenv
		возврат:
			nil или таблица с данными, например
				1	table: 0x802215e00
				3	table: 0x802215e40
				6	table: 0x802215ec0

	get(string dbname, string path, key)
		описание:
			получить значения из бд по ключу
		параметры:
			string - имя бд
			string - путь до папки с бд. может быть nil, тогда путь будет взят из kenv
			key - ключ под которым будут получены данные
		возврат:
			nil или таблица с данными, например
				name	John
				age		100

    del(string dbname, string path, int key);
		описание:
			удалить значение из бд
		параметры:
			string - имя бд
			string - путь до папки с бд. может быть nil, тогда путь будет взят из kenv
			key - ключ под которым будут удалены данные
		возврат:
			nil/true

	schema(string dbname, string path);
		описание:
			получить схему бд
		параметры:
			string - имя бд
			string - путь до папки с бд. может быть nil, тогда путь будет взят из kenv
		возврат:
			nil или таблица с данными, например
				hobbies	S
				age		l
				name	s

    check();
		описание:
			пишет в консоль сообщение "BDB Library loaded". всегда возвращает true
		возврат:
			true

