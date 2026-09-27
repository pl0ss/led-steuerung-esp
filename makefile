.PHONY: app_angular_ionic aa app_angular_ionic_local app_angular_ionic_ios aai app_angular_ionic_android aaa app_react_ionic ar app_react_ionic_local app_react_ionic_ios ari app_react_ionic_android ara

app_angular_ionic:
	cd ./apps/angular-ionic && ionic serve

aa: app_angular_ionic

app_angular_ionic_local:
	cd ./apps/angular-ionic && ionic serve --host 0.0.0.0 --port 8100

app_angular_ionic_ios:
	cd ./apps/angular-ionic && npm run build:ios && npx cap open ios

aai: app_angular_ionic_ios

app_angular_ionic_android:
	cd ./apps/angular-ionic && npm run build:android && npx cap open android

aaa: app_angular_ionic_android


app_react_ionic:
	cd ./apps/react-ionic && ionic serve

ar: app_react_ionic

app_react_ionic_local:
	cd ./apps/react-ionic && ionic serve --host 0.0.0.0 --port 8100

app_react_ionic_ios:
	cd ./apps/react-ionic && npm run build:ios && npx cap open ios

ari: app_react_ionic_ios

app_react_ionic_android:
	cd ./apps/react-ionic && npm run build:android && npx cap open android

ara: app_react_ionic_android