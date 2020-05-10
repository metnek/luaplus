REQUIREMENTS
	lua-5.3 or >, libm (standard)
	PREFIX=/usr/local/lib/lua/5.3
	CFLAGS+= -I/usr/local/include/lua53 -fsanitize=safe-stack
	LDFLAGS+= -L/usr/local/lib

DESCRIPTION
	libluaposix_shm.so - модуль lua для работы с POSIX shared memory

FUNCTIONS
	attach(name, size, mode);
		описание:
			создать или открыть область общей памяти
		параметры:
			name - имя области
			size - размер области
			mode - режим создания/открытия (по умолчанию 0660)
		возврат:
			false или таблица с данными для доступа к памяти

	read(table);
		описание:
			получить данные из общей памяти
		параметры:
			table - таблица с данными для доступа к памяти
		возврат:
			false или string

	write(data, table);
		описание:
			записать данные в общую память
		параметры:
			data - данные для записи (string)
                        table - таблица с данными для доступа к памяти
		возврат:
			true/false

	close(table);
		описание:
			закрыть и освободить общую память
                параметры:
                        table - таблица с данными для доступа к памяти
		возврат:
                        true/false

EXAMPLES
	> shm = require('libluaposix_shm');
	> table = shm.attach('name', 256)
	> shm.write('hello world!', table)
	> print(shm.read(table))
	hello world!
	> shm.close(table)


