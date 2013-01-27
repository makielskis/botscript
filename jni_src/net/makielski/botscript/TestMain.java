package net.makielski.botscript;

public class TestMain {
	public static void main(String[] args) {
		
		// Load packages.
		Bot.loadPackages("packages");
		
		// Create bot.
		final Bot b = new Bot(new Bot.AsyncHandler() {
			@Override
			public void call(String error) {
				// Split at delimiter.
				String[] errSplit = error.split("\\|");
				
				// Check split.
				if (errSplit.length != 3) {
					System.err.println("invalid message: " + error);
					return;
				}
				
				// Print log message.
				String k = errSplit[1], v = errSplit[2];
				if (k.equals("log")) {
					System.out.print(v);
				}
			}
		});
		
		// Load bot configuration.
		b.load("{"+
				"	\"username\": \"oclife\","+
				"	\"password\": \"blabla\","+
				"	\"package\": \"packages/pg\","+
				"	\"server\": \"http://sylt.pennergame.de\","+
				"	\"modules\": {"+
				"		\"fight\": {"+
				"			\"active\": \"0\","+
				"			\"auto\": \"0\","+
				"			\"blocklist\": \"\","+
				"			\"victims\": \"\""+
				"		},"+
				"		\"crime\": {"+
				"			\"active\": \"0\","+
				"			\"crime\": \"\","+
				"			\"crime_from\": \"\""+
				"		},"+
				"		\"study\": {"+
				"			\"active\": \"0\","+
				"			\"alcohol\": \"0\","+
				"			\"training_index\": \"1\","+
				"			\"trainings\": \",\","+
				"			\"trainings_from\": \"att,def,agi,sprechen,bildungsstufe,musik,sozkontakte,konzentration,pickpocket\""+
				"		},"+
				"		\"sell\": {"+
				"			\"active\": \"1\","+
				"			\"amount\": \"10\","+
				"			\"continuous\": \"0\","+
				"			\"price\": \"30\""+
				"		},"+
				"		\"collect\": {"+
				"			\"active\": \"1\""+
				"		},"+
				"		\"base\": {"+
				"			\"wait_time_factor\": \"1.00\","+
				"			\"proxy\": \"\""+
				"		}"+
				"	}"+
				"}", new Bot.AsyncHandler() {
			@Override
			public void call(String error) {
				if (error.isEmpty()) {
					b.execute("base_set_wait_time_factor", "2.0");
					System.out.println("IDENTIFIER: " + b.identifier());
					System.out.println("CONFIGURATION: " + b.configuration(true));
					System.out.println("USERNAME: " + b.u());
					System.out.println("PACKAGE: " + b.p());
					System.out.println("SERVER: " + b.s());
				} else {
					System.out.println("ERROR: " + error);
				}
			}
		});
		
		try {
			Thread.sleep(Long.MAX_VALUE);
		} catch (InterruptedException e) {
		}
		
		b.shutdown();
		
		try {
			Thread.sleep(Long.MAX_VALUE);
		} catch (InterruptedException e) {
		}
	}
}
