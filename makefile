.PHONY: app_angular_ionic fa app_angular_ionic_local app_angular_ionic_ios fai

app_angular_ionic:
	cd ./apps/angular-ionic && ionic serve

fa: app_angular_ionic

app_angular_ionic_local:
	cd ./apps/angular-ionic && ionic serve --host 0.0.0.0 --port 8100

app_angular_ionic_ios:
	cd ./apps/angular-ionic && npm run build:ios && npx cap open ios

fai: app_angular_ionic_ios

app_angular_ionic_android:
	cd ./apps/angular-ionic && npm run build:android && npx cap open android

faa: app_angular_ionic_android