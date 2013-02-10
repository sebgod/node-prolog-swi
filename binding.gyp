{
  'targets': [
	{
	  'target_name': 'libswipl',
	  'sources': [ 'src/libswipl.cc' ],
	  'include_dirs': ["C:\Program Files\swipl\include"],
	  
       'conditions': [
        ['OS=="win"', {
          'libraries': ['C:\Program Files\swipl\lib\libswipl.dll.a']
		  }
		]
	  ]
	}
  ]
}
