// fiiir.com
float coeff_bandpass[] = {
    0.000000036462928354,
    -0.000000003776861285,
    -0.000000134991927643,
    -0.000000322879115718,
    -0.000000488543057060,
    -0.000000530618431674,
    -0.000000362410444985,
    0.000000045083046344,
    0.000000628806811342,
    0.000001224594243516,
    0.000001596289499189,
    0.000001505175161109,
    0.000000805962354497,
    -0.000000460438716797,
    -0.000002016397893470,
    -0.000003374365365841,
    -0.000003956118652972,
    -0.000003286892203981,
    -0.000001211075945061,
    0.000001948790086931,
    0.000005368821153518,
    0.000007885023625627,
    0.000008329442660288,
    0.000005961525986279,
    0.000000857227150657,
    -0.000005901386601122,
    -0.000012357958689273,
    -0.000016166878705762,
    -0.000015339560226370,
    -0.000009041543328552,
    0.000001837583079431,
    0.000014599716043064,
    0.000025278722249040,
    0.000029749993248851,
    0.000025136576474390,
    0.000011075912784566,
    -0.000009629174205073,
    -0.000031329777746449,
    -0.000046902857445735,
    -0.000049903791524889,
    -0.000036901274596358,
    -0.000009219212533896,
    0.000026627561448815,
    0.000060193729355445,
    0.000079932344355709,
    0.000076898856016462,
    0.000048213038880478,
    -0.000000948445213599,
    -0.000057759634149343,
    -0.000104814335632266,
    -0.000125244655644062,
    -0.000108468100059322,
    -0.000054689052510985,
    0.000023600790307865,
    0.000104132765990115,
    0.000161172009639000,
    0.000174065023874318,
    0.000135417871573378,
    0.000055977883964027,
    -0.000036075186824415,
    -0.000102190393449245,
    -0.000105559620024367,
    -0.000404783734285747,
    -0.000278958839959241,
    0.000025277051251468,
    0.000425554220190449,
    0.000793673618369595,
    0.000987408867755079,
    0.000892938443460066,
    0.000468454988789713,
    -0.000224378386801627,
    -0.001015258992276848,
    -0.001656131128029005,
    -0.001887881659557389,
    -0.001529012715646257,
    -0.000559853372988280,
    0.000830089304609080,
    0.002258847941505280,
    0.003242637914850811,
    0.003344619039584286,
    0.002340235859204605,
    0.000341981329878953,
    -0.002170715635377142,
    -0.004446703506048815,
    -0.005673400711164009,
    -0.005248013475820251,
    -0.003023493642313233,
    0.000562767882941081,
    0.004542613076717616,
    0.007663375369091006,
    0.008769968710550729,
    0.007209621171098204,
    0.003122096354749364,
    -0.002500277576767081,
    -0.008033351910786180,
    -0.011682294318804938,
    -0.012055028833497052,
    -0.008666667254796982,
    -0.002197282490866080,
    0.005609414755593927,
    0.012408638009066240,
    0.015958855907198418,
    0.014856268659944192,
    0.009059232542550821,
    0.000012476768020909,
    -0.009701147541280677,
    -0.017102675636809325,
    -0.019738575347160010,
    -0.016494152573363709,
    -0.008028319944207808,
    0.003333178475554077,
    0.014241639485729622,
    0.021327131493328894,
    0.022254080130740744,
    0.016495481990833807,
    0.005567185683320005,
    -0.007372592676211836,
    -0.018452959398403371,
    -0.024270849826050576,
    -0.022955874308466323,
    -0.014780649049370990,
    -0.002114934808241683,
    0.011273473346629126,
    0.021361025236296162,
    0.025100926859369259,
    0.021361025236295232,
    0.011273473346628857,
    -0.002114934808241064,
    -0.014780649049370896,
    -0.022955874308466222,
    -0.024270849826050430,
    -0.018452959398403444,
    -0.007372592676211797,
    0.005567185683320140,
    0.016495481990833734,
    0.022254080130740636,
    0.021327131493328773,
    0.014241639485729414,
    0.003333178475554069,
    -0.008028319944207737,
    -0.016494152573363674,
    -0.019738575347159882,
    -0.017102675636809228,
    -0.009701147541280725,
    0.000012476768020903,
    0.009059232542550814,
    0.014856268659944157,
    0.015958855907198411,
    0.012408638009066206,
    0.005609414755593870,
    -0.002197282490866083,
    -0.008666667254796980,
    -0.012055028833497034,
    -0.011682294318804874,
    -0.008033351910786173,
    -0.002500277576767077,
    0.003122096354749360,
    0.007209621171098178,
    0.008769968710550691,
    0.007663375369090994,
    0.004542613076717592,
    0.000562767882941077,
    -0.003023493642313228,
    -0.005248013475820251,
    -0.005673400711163991,
    -0.004446703506048777,
    -0.002170715635377126,
    0.000341981329878950,
    0.002340235859204596,
    0.003344619039584268,
    0.003242637914850800,
    0.002258847941505278,
    0.000830089304609081,
    -0.000559853372988265,
    -0.001529012715646244,
    -0.001887881659557396,
    -0.001656131128029006,
    -0.001015258992276844,
    -0.000224378386801632,
    0.000468454988789707,
    0.000892938443460063,
    0.000987408867755077,
    0.000793673618369592,
    0.000425554220190441,
    0.000025277051251469,
    -0.000278958839959232,
    -0.000404783734285753,
    -0.000105559620024358,
    -0.000102190393449238,
    -0.000036075186824407,
    0.000055977883964034,
    0.000135417871573368,
    0.000174065023874313,
    0.000161172009639002,
    0.000104132765990114,
    0.000023600790307853,
    -0.000054689052510987,
    -0.000108468100059311,
    -0.000125244655644057,
    -0.000104814335632269,
    -0.000057759634149342,
    -0.000000948445213590,
    0.000048213038880475,
    0.000076898856016452,
    0.000079932344355706,
    0.000060193729355453,
    0.000026627561448818,
    -0.000009219212533903,
    -0.000036901274596360,
    -0.000049903791524886,
    -0.000046902857445735,
    -0.000031329777746452,
    -0.000009629174205071,
    0.000011075912784576,
    0.000025136576474394,
    0.000029749993248847,
    0.000025278722249040,
    0.000014599716043068,
    0.000001837583079430,
    -0.000009041543328546,
    -0.000015339560226365,
    -0.000016166878705752,
    -0.000012357958689264,
    -0.000005901386601121,
    0.000000857227150659,
    0.000005961525986282,
    0.000008329442660284,
    0.000007885023625617,
    0.000005368821153514,
    0.000001948790086932,
    -0.000001211075945060,
    -0.000003286892203986,
    -0.000003956118652969,
    -0.000003374365365828,
    -0.000002016397893466,
    -0.000000460438716800,
    0.000000805962354495,
    0.000001505175161121,
    0.000001596289499183,
    0.000001224594243497,
    0.000000628806811327,
    0.000000045083046336,
    -0.000000362410444995,
    -0.000000530618431683,
    -0.000000488543057054,
    -0.000000322879115700,
    -0.000000134991927626,
    -0.000000003776861277,
    0.000000036462928365
};


/*
float coeff_bandpass[] = {
    -0.000049667891008862,
    -0.000082505495149508,
    -0.000082419725815841,
    -0.000042564605141018,
    0.000031750886731946,
    0.000123628830704511,
    0.000208683928576606,
    0.000261700248992016,
    0.000264044309628554,
    0.000209692356454297,
    0.000107990398357052,
    -0.000017907552011543,
    -0.000136787319490411,
    -0.000217938496299906,
    -0.000240237949491669,
    -0.000199124134982942,
    -0.000109232010858879,
    -0.000001527119759525,
    0.000084736252823148,
    0.000113390071481858,
    0.000063089300699269,
    -0.000063958305670638,
    -0.000239138791031772,
    -0.000411995056264291,
    -0.000521881217561321,
    -0.000514437759131790,
    -0.000359097494486809,
    -0.000063164337554578,
    0.000321579280524773,
    0.000702318619586611,
    0.000959079945854949,
    0.000966951820742127,
    0.000622464464258271,
    -0.000131307217960568,
    -0.001286398735524864,
    -0.002762650578780128,
    0.002830378530404382,
    -0.002519970638830962,
    -0.008923262182541074,
    -0.014740624153076236,
    -0.018331764976970623,
    -0.018516117685207851,
    -0.014932120797064536,
    -0.008185525148043878,
    0.000268012410851050,
    0.008487653030118019,
    0.014569017602758257,
    0.017182073296091019,
    0.015971559538961982,
    0.011696240489674676,
    0.006050238002762203,
    0.001196867984785052,
    -0.000870561751374628,
    0.001030647709703133,
    0.006815017628835920,
    0.015000984997418349,
    0.022978295097917122,
    0.027631775960962515,
    0.026181557330637265,
    0.017029869365863586,
    0.000386325021703308,
    -0.021515141340570039,
    -0.044701374113571932,
    -0.064217527677578934,
    -0.075204531528552832,
    -0.074053292757130351,
    -0.059358640520375372,
    -0.032430104508599761,
    0.002787410432163656,
    0.040394064931157714,
    0.073684408281848995,
    0.096518060010860357,
    0.104605886405147855,
    0.096518060010861578,
    0.073684408281849204,
    0.040394064931157297,
    0.002787410432163698,
    -0.032430104508599664,
    -0.059358640520375344,
    -0.074053292757130046,
    -0.075204531528552748,
    -0.064217527677579059,
    -0.044701374113571862,
    -0.021515141340569949,
    0.000386325021703313,
    0.017029869365863644,
    0.026181557330637185,
    0.027631775960962331,
    0.022978295097917097,
    0.015000984997418327,
    0.006815017628835875,
    0.001030647709703207,
    -0.000870561751374611,
    0.001196867984785021,
    0.006050238002762238,
    0.011696240489674690,
    0.015971559538961975,
    0.017182073296091092,
    0.014569017602758260,
    0.008487653030117972,
    0.000268012410851050,
    -0.008185525148043885,
    -0.014932120797064498,
    -0.018516117685207729,
    -0.018331764976970581,
    -0.014740624153076257,
    -0.008923262182541058,
    -0.002519970638830967,
    0.002830378530404359,
    -0.002762650578780139,
    -0.001286398735524867,
    -0.000131307217960562,
    0.000622464464258268,
    0.000966951820742125,
    0.000959079945854949,
    0.000702318619586618,
    0.000321579280524779,
    -0.000063164337554574,
    -0.000359097494486804,
    -0.000514437759131772,
    -0.000521881217561316,
    -0.000411995056264300,
    -0.000239138791031777,
    -0.000063958305670631,
    0.000063089300699271,
    0.000113390071481856,
    0.000084736252823151,
    -0.000001527119759520,
    -0.000109232010858877,
    -0.000199124134982944,
    -0.000240237949491669,
    -0.000217938496299902,
    -0.000136787319490406,
    -0.000017907552011532,
    0.000107990398357060,
    0.000209692356454315,
    0.000264044309628568,
    0.000261700248992012,
    0.000208683928576599,
    0.000123628830704517,
    0.000031750886731944,
    -0.000042564605141022,
    -0.000082419725815837,
    -0.000082505495149500,
    -0.000049667891008860
};
*/

float coeff_lowpass[] = {
    0.000000168811312499,
    0.000000052240190743,
    0.000000055513520884,
    0.000000193039119581,
    0.000000000000000009,
    0.000000157219714254,
    0.000000165370290661,
    -0.000000008453948717,
    0.000000278970074675,
    0.000000068390682262,
    0.000000072859700225,
    0.000000346895492779,
    -0.000000068811391256,
    0.000000258932814021,
    0.000000260156527329,
    -0.000000147471194131,
    0.000000469651906895,
    -0.000000028041162939,
    -0.000000049852145441,
    0.000000517541517645,
    -0.000000423542883894,
    0.000000240882234035,
    0.000000202313074144,
    -0.000000692513868179,
    0.000000537971884279,
    -0.000000517632744228,
    -0.000000603253426292,
    0.000000481323796164,
    -0.000001399646434869,
    -0.000000148852634346,
    -0.000000254014993095,
    -0.000001975828366601,
    0.000000311727720297,
    -0.000001666788130270,
    -0.000001832636303470,
    0.000000155295005277,
    -0.000003262573901959,
    -0.000000976446992347,
    -0.000001137144051127,
    -0.000004196843093513,
    -0.000000031973942062,
    -0.000003521764887940,
    -0.000003740870480750,
    -0.000000062255617851,
    -0.000006143427854595,
    -0.000001853667480561,
    -0.000002008133727089,
    -0.000007530974231765,
    0.000000480843604032,
    -0.000006001324566060,
    -0.000006216086728161,
    0.000001382817303703,
    -0.000010918617313423,
    -0.000001440924382023,
    -0.000001483970005829,
    -0.000014319709557674,
    0.000006644601303322,
    -0.000011333690291434,
    -0.000011559462354896,
    0.000018840574896337,
    -0.000045716991361979,
    0.000045517302597632,
    0.000765808849481815,
    0.000162787379764699,
    0.000413866358856811,
    0.000791403544207426,
    0.000002012316884244,
    0.000878180773014105,
    0.000558348385652563,
    0.000136288919986080,
    0.001307338955105710,
    0.000121845481195581,
    0.000681471011462035,
    0.001377412575856756,
    -0.000294756357527681,
    0.001554485732622857,
    0.000754454272844400,
    -0.000253092835033493,
    0.002248359554653821,
    -0.000542499600064860,
    0.000579697891929650,
    0.001934220594010596,
    -0.001892570704107649,
    0.001929180382992074,
    -0.000000000000000038,
    -0.002304028924204275,
    0.002676485569856989,
    -0.003264665174986787,
    -0.001178627600037154,
    0.001297726889744147,
    -0.006427500562023175,
    0.000842374543939898,
    -0.003060496609130474,
    -0.007568706981102421,
    0.001691454923881258,
    -0.009509945897572054,
    -0.005682140488358523,
    -0.001171253328969965,
    -0.015261652022056246,
    -0.001978818394213161,
    -0.008942376232624226,
    -0.016893022556861537,
    -0.000012111965674363,
    -0.019861741528660940,
    -0.012631581432208192,
    -0.004077962965158192,
    -0.029061051099697033,
    -0.004376421955666591,
    -0.016426771957764590,
    -0.030449298156175718,
    0.001957650193089900,
    -0.034757228581941607,
    -0.019949987810125788,
    -0.001712213135042357,
    -0.051465088978422641,
    0.001645271249043602,
    -0.022811771590462930,
    -0.054603929462596096,
    0.028023921176175182,
    -0.068657447145999001,
    -0.024680187438279821,
    0.049635599416408493,
    -0.183880443402219373,
    0.199566285113782022,
    0.724365747529389203,
    0.199566285113780884,
    -0.183880443402218902,
    0.049635599416408660,
    -0.024680187438279960,
    -0.068657447145998404,
    0.028023921176175120,
    -0.054603929462596193,
    -0.022811771590462715,
    0.001645271249043633,
    -0.051465088978422426,
    -0.001712213135042364,
    -0.019949987810125781,
    -0.034757228581941364,
    0.001957650193089920,
    -0.030449298156175815,
    -0.016426771957764576,
    -0.004376421955666555,
    -0.029061051099696918,
    -0.004077962965158228,
    -0.012631581432208225,
    -0.019861741528660801,
    -0.000012111965674358,
    -0.016893022556861613,
    -0.008942376232624210,
    -0.001978818394213146,
    -0.015261652022056219,
    -0.001171253328969975,
    -0.005682140488358561,
    -0.009509945897571993,
    0.001691454923881251,
    -0.007568706981102426,
    -0.003060496609130390,
    0.000842374543939908,
    -0.006427500562023155,
    0.001297726889744161,
    -0.001178627600037165,
    -0.003264665174986754,
    0.002676485569856986,
    -0.002304028924204278,
    -0.000000000000000011,
    0.001929180382992056,
    -0.001892570704107651,
    0.001934220594010595,
    0.000579697891929626,
    -0.000542499600064860,
    0.002248359554653817,
    -0.000253092835033492,
    0.000754454272844432,
    0.001554485732622848,
    -0.000294756357527698,
    0.001377412575856757,
    0.000681471011462014,
    0.000121845481195571,
    0.001307338955105712,
    0.000136288919986085,
    0.000558348385652564,
    0.000878180773014102,
    0.000002012316884238,
    0.000791403544207433,
    0.000413866358856804,
    0.000162787379764698,
    0.000765808849481797,
    0.000045517302597645,
    -0.000045716991361966,
    0.000018840574896360,
    -0.000011559462354910,
    -0.000011333690291437,
    0.000006644601303312,
    -0.000014319709557685,
    -0.000001483970005821,
    -0.000001440924382032,
    -0.000010918617313419,
    0.000001382817303697,
    -0.000006216086728170,
    -0.000006001324566065,
    0.000000480843604018,
    -0.000007530974231774,
    -0.000002008133727077,
    -0.000001853667480567,
    -0.000006143427854606,
    -0.000000062255617851,
    -0.000003740870480754,
    -0.000003521764887940,
    -0.000000031973942062,
    -0.000004196843093516,
    -0.000001137144051123,
    -0.000000976446992341,
    -0.000003262573901943,
    0.000000155295005283,
    -0.000001832636303474,
    -0.000001666788130260,
    0.000000311727720282,
    -0.000001975828366607,
    -0.000000254014993077,
    -0.000000148852634336,
    -0.000001399646434856,
    0.000000481323796174,
    -0.000000603253426309,
    -0.000000517632744224,
    0.000000537971884285,
    -0.000000692513868180,
    0.000000202313074152,
    0.000000240882234035,
    -0.000000423542883883,
    0.000000517541517652,
    -0.000000049852145451,
    -0.000000028041162936,
    0.000000469651906901,
    -0.000000147471194131,
    0.000000260156527334,
    0.000000258932814026,
    -0.000000068811391247,
    0.000000346895492793,
    0.000000072859700222,
    0.000000068390682272,
    0.000000278970074673,
    -0.000000008453948721,
    0.000000165370290691,
    0.000000157219714248,
    0.000000000000000005,
    0.000000193039119614,
    0.000000055513520873,
    0.000000052240190784,
    0.000000168811312512
};
