#!/usr/bin/env python3

import sys
import discord

MY_GUILD = discord.Object(id=529770457033998346)


class lemonpi(discord.Client):
	def __init__(self, *, intents: discord.Intents):
		super().__init__(intents=intents)
		self.tree = discord.app_commands.CommandTree(self)

	async def on_ready(self):
		print("connected")

	async def setup_hook(self):
		self.tree.copy_global_to(guild=MY_GUILD)
		await self.tree.sync(guild=MY_GUILD)


intents = discord.Intents.default()
client = lemonpi(intents=intents)

@client.tree.command()
@discord.app_commands.describe()
async def ping(interaction: discord.Interaction, user: discord.Member):
	await interaction.response.send_message(f"oi <@{user.id}>!")

def main():
	if len(sys.argv) != 2:
		print("usage: lemonpi.py <token>")
		exit(-1)

	token = sys.argv[1]

	client.run(token)


if __name__ == "__main__":
	main()
